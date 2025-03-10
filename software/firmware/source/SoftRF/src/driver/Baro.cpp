/*
 * BaroHelper.cpp
 * Copyright (C) 2018-2021 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../system/SoC.h"

#include "Baro.h"

// including BMP180 & MPL3115A2 still hangs at probe() even with ESP32 Core 2.0.3
// #define EXCLUDE_BMP280
#define EXCLUDE_BMP180
#define EXCLUDE_MPL3115A2

barochip_ops_t *baro_chip = NULL;

#if defined(EXCLUDE_BMP180) && defined(EXCLUDE_BMP280) && defined(EXCLUDE_MPL3115A2)
byte  Baro_setup()        {return BARO_MODULE_NONE;}
bool  Baro_probe()        {return false;}
void  Baro_loop()         {}
float Baro_altitude()     {return 0;}
float Baro_pressure()     {return 0;}
float Baro_temperature()  {return 0;}

#else

#if !defined(EXCLUDE_BMP180)
#include <Adafruit_BMP085.h>
#endif /* EXCLUDE_BMP180 */
#if !defined(EXCLUDE_BMP280)
#include <Adafruit_BMP280.h>
#endif /* EXCLUDE_BMP280 */
#if !defined(EXCLUDE_MPL3115A2)
#include <Adafruit_MPL3115A2.h>
#endif /* EXCLUDE_MPL3115A2 */

#include <TinyGPS++.h>

#if !defined(EXCLUDE_BMP180)
Adafruit_BMP085 bmp180;
#endif /* EXCLUDE_BMP180 */
#if !defined(EXCLUDE_BMP280)
Adafruit_BMP280 bmp280;
#endif /* EXCLUDE_BMP280 */
#if !defined(EXCLUDE_MPL3115A2)
Adafruit_MPL3115A2 mpl3115a2 = Adafruit_MPL3115A2();
#endif /* EXCLUDE_MPL3115A2 */

static float Baro_altitude_cache            = 0;
static float Baro_pressure_cache            = 0;
static float Baro_temperature_cache         = 0;

static unsigned long BaroAltitudeTimeMarker = 0;
static unsigned long BaroPresTempTimeMarker = 0;
static float prev_pressure_altitude         = 0;

static float Baro_VS[VS_AVERAGING_FACTOR];
static int avg_ndx = 0;

#if !defined(EXCLUDE_BMP180)

static bool bmp180_probe()
{
//  return bmp180.begin();
  bmp180.setWire(&Wire);
  if (bmp180.begin())
      return true;
#if defined(ESP32)
  if (hw_info.model == SOFTRF_MODEL_PRIME_MK2) {
      bmp180.setWire(&Wire1);
      if (bmp180.begin())
          return true;
  }
#endif
  return false;
}

static void bmp180_setup()
{
  Serial.print(F("Temperature = "));
  Serial.print(bmp180.readTemperature());
  Serial.println(F(" *C"));
  
  Serial.print(F("Pressure = "));
  Serial.print(bmp180.readPressure());
  Serial.println(F(" Pa"));
  
  // Calculate altitude assuming 'standard' barometric
  // pressure of 1013.25 millibar = 101325 Pascal
  Serial.print(F("Altitude = "));
  Serial.print(bmp180.readAltitude());
  Serial.println(F(" meters"));
  
  Serial.println();
  delay(500);
}

static float bmp180_altitude(float sealevelPressure)
{
  return bmp180.readAltitude(sealevelPressure * 100);
}

static float bmp180_pressure()
{
  return (float) bmp180.readPressure();
}

static float bmp180_temperature()
{
  return bmp180.readTemperature();
}

barochip_ops_t bmp180_ops = {
  BARO_MODULE_BMP180,
  "BMP180",
  bmp180_probe,
  bmp180_setup,
  bmp180_altitude,
  bmp180_pressure,
  bmp180_temperature
};
#endif /* EXCLUDE_BMP180 */

#if !defined(EXCLUDE_BMP280)
static bool bmp280_probe()
{
// is there a problem with probing both Wires on oldest T-Beams?
//    if (hw_info.revision == 2)
//      return false;
  bmp280.setWire(&Wire);
  if (bmp280.begin(BMP280_ADDRESS,     BMP280_CHIPID))  return true;
  if (bmp280.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID))  return true;
  if (bmp280.begin(BMP280_ADDRESS,     BME280_CHIPID))  return true;
  if (bmp280.begin(BMP280_ADDRESS_ALT, BME280_CHIPID))  return true;
#if defined(ESP32)
  if (hw_info.model == SOFTRF_MODEL_PRIME_MK2) {
      Serial.println(F("BMP280 not found on pins 2,13..."));
      Serial.println(F("probing BMP280 on pins 21,22..."));
      bmp280.setWire(&Wire1);
      if (bmp280.begin(BMP280_ADDRESS,     BMP280_CHIPID))  return true;
      if (bmp280.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID))  return true;
      if (bmp280.begin(BMP280_ADDRESS,     BME280_CHIPID))  return true;
      if (bmp280.begin(BMP280_ADDRESS_ALT, BME280_CHIPID))  return true;
  }
#endif
  return false;
}

static void bmp280_setup()
{
    Serial.print(F("Temperature = "));
    Serial.print(bmp280.readTemperature());
    Serial.println(F(" *C"));
    
    Serial.print(F("Pressure = "));
    Serial.print(bmp280.readPressure());
    Serial.println(F(" Pa"));

    Serial.print(F("Approx altitude = "));
    Serial.print(bmp280.readAltitude(1013.25)); // this should be adjusted to your local forcase
    Serial.println(F(" m"));
    
    Serial.println();
    delay(500);
}

static float bmp280_altitude(float sealevelPressure)
{
    return bmp280.readAltitude(sealevelPressure);
}

static float bmp280_pressure()
{
    return bmp280.readPressure();
}

static float bmp280_temperature()
{
    return bmp280.readTemperature();
}

barochip_ops_t bmp280_ops = {
  BARO_MODULE_BMP280,
  "BMP280",
  bmp280_probe,
  bmp280_setup,
  bmp280_altitude,
  bmp280_pressure,
  bmp280_temperature
};
#endif /* EXCLUDE_BMP280 */

#if !defined(EXCLUDE_MPL3115A2)
static bool mpl3115a2_probe()
{
//  return mpl3115a2.begin();
  if (mpl3115a2.begin(&Wire))
    return true;
#if defined(ESP32)
  if (mpl3115a2.begin(&Wire1))
    return true;
#endif
  return false;
}

static void mpl3115a2_setup()
{
  float pascals = mpl3115a2.getPressure();
  // Our weather page presents pressure in Inches (Hg)
  // Use http://www.onlineconversion.com/pressure.htm for other units
  Serial.print(pascals/3377); Serial.println(F(" Inches (Hg)"));

  float altm = mpl3115a2.getAltitude();
  Serial.print(altm); Serial.println(F(" meters"));

  float tempC = mpl3115a2.getTemperature();
  Serial.print(tempC); Serial.println(F("*C"));

  delay(250);
}

static float mpl3115a2_altitude(float sealevelPressure)
{
  mpl3115a2.setSeaPressure(sealevelPressure * 100);
  return mpl3115a2.getAltitude();
}

static float mpl3115a2_pressure()
{
  return mpl3115a2.getPressure();
}

static float mpl3115a2_temperature()
{
  return mpl3115a2.getTemperature();
}

barochip_ops_t mpl3115a2_ops = {
  BARO_MODULE_MPL3115A2,
  "MPL3115A2",
  mpl3115a2_probe,
  mpl3115a2_setup,
  mpl3115a2_altitude,
  mpl3115a2_pressure,
  mpl3115a2_temperature
};
#endif /* EXCLUDE_MPL3115A2 */

bool Baro_probe()
{

#if !defined(EXCLUDE_BMP180)
  baro_chip = &bmp180_ops;
  if (baro_chip->probe())  return true;
#endif /* EXCLUDE_BMP180 */

#if !defined(EXCLUDE_BMP280)
  baro_chip = &bmp280_ops;
  if (baro_chip->probe())  return true;
#endif /* EXCLUDE_BMP280 */

#if !defined(EXCLUDE_MPL3115A2)
  baro_chip = &mpl3115a2_ops;
  if (baro_chip->probe())  return true;
#endif /* EXCLUDE_MPL3115A2 */

  baro_chip = NULL;
  return false;
}

byte Baro_setup()
{
Serial.println("baro setting up");

  if ( SoC->Baro_setup() /* && Baro_probe() */ ) {    // Baro_probe() also called from inside ESP32_Baro_setup()

    if (baro_chip == NULL) {
        Serial.println(F("BUG WARNING! Baro_probe() was not called"));
        return BARO_MODULE_NONE;
    }

    Serial.print(baro_chip->name);
    Serial.println(F(" barometric pressure sensor is detected."));

    baro_chip->setup();

    Baro_pressure_cache    = baro_chip->pressure();
    Baro_temperature_cache = baro_chip->temperature();
    BaroPresTempTimeMarker = millis();

    Baro_altitude_cache    = baro_chip->altitude(1013.25);
    ThisAircraft.pressure_altitude = prev_pressure_altitude = Baro_altitude_cache;
    BaroAltitudeTimeMarker = millis();

    for (int i=0; i<VS_AVERAGING_FACTOR; i++) {
      Baro_VS[i] = 0;
    }

    return baro_chip->type;

  } else {
    baro_chip = NULL;
    Serial.println(F("Barometric pressure sensor was NOT detected."));
    return BARO_MODULE_NONE;
  }
}

void Baro_loop()
{
  if (baro_chip == NULL) return;

  if (isTimeToBaroAltitude()) {

    /* Draft of pressure altitude and vertical speed calculation */
    Baro_altitude_cache = baro_chip->altitude(1013.25);

    ThisAircraft.pressure_altitude = Baro_altitude_cache;
    ThisAircraft.baro_alt_diff = ThisAircraft.altitude - ThisAircraft.pressure_altitude;

    Baro_VS[avg_ndx] = (Baro_altitude_cache - prev_pressure_altitude) /
                       (millis() - BaroAltitudeTimeMarker) * 1000;  /* in m/s */

    ThisAircraft.vs = 0;
    for (int i=0; i<VS_AVERAGING_FACTOR; i++) {
      ThisAircraft.vs += Baro_VS[i];
    }
    ThisAircraft.vs /= VS_AVERAGING_FACTOR;

    if (ThisAircraft.vs > -0.1 && ThisAircraft.vs < 0.1) {
      ThisAircraft.vs = 0;
    }

    ThisAircraft.vs *= (_GPS_FEET_PER_METER * 60.0) ; /* feet per minute */

    prev_pressure_altitude = Baro_altitude_cache;
    BaroAltitudeTimeMarker = millis();
    avg_ndx = (avg_ndx + 1) % VS_AVERAGING_FACTOR;

#if 0
    Serial.print(F("P.Alt. = ")); Serial.print(ThisAircraft.pressure_altitude);
    Serial.print(F(" , VS avg. = ")); Serial.println(ThisAircraft.vs);
#endif
  }

  if (isTimeToBaroPresTemp()) {
    Baro_pressure_cache    = baro_chip->pressure();
    Baro_temperature_cache = baro_chip->temperature();
    BaroPresTempTimeMarker = millis();
  }
}

float Baro_altitude()
{
  return Baro_altitude_cache;
}

float Baro_pressure()
{
  return Baro_pressure_cache;
}

float Baro_temperature()
{
  return Baro_temperature_cache;
}

#endif /* EXCLUDE_BMP180 && EXCLUDE_BMP280 && EXCLUDE_MPL3115A2 */
