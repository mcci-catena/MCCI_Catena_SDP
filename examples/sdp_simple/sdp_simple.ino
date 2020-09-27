/*

Module: sdp-simple.ino

Function:
        Simple example for SDP sensors.

Copyright and License:
        See accompanying LICENSE file.

Author:
        Terry Moore, MCCI Corporation   September 2020

*/

#include <MCCI_Catena_SDP.h>

#include <Arduino.h>
#include <Wire.h>

/****************************************************************************\
|
|   Manifest constants & typedefs.
|
\****************************************************************************/

using namespace McciCatenaSdp;

/****************************************************************************\
|
|   Read-only data.
|
\****************************************************************************/

#if defined(MCCI_CATENA_4801)
static constexpr bool k4801 = true;
#else
static constexpr bool k4801 = false;
#endif

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

bool fLed;
cSDP gSdp {Wire, cSDP::Address::SDP8xx};

/****************************************************************************\
|
|   Code.
|
\****************************************************************************/

void printFailure(const char *pMessage)
    {
    for (;;)
        {
        Serial.print(pMessage);
        Serial.print(", error: ");
        Serial.println(std::uint8_t(gSdp.getLastError()));
        delay(2000);
        }
    
    }
void setup()
    {
    Serial.begin(115200);

    // wait for USB to be attached.
    while (! Serial)
        yield();

    Serial.println("SDP Simple Test");

    if (k4801)
        {
        /* power up sensor */
        pinMode(D11, OUTPUT);
        digitalWrite(D11, 1);

        /* power up other i2c sensors */
        pinMode(D10, OUTPUT);
        digitalWrite(D10, 1);

        /* bring up the LED */
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, 1);
        fLed = true;
        }

    // let message get out.
    delay(1000);

    if (! gSdp.begin())
        {
        printFailure("gSdp.begin() failed");
        }
    }

void loop()
    {
    fLed = !fLed;
    digitalWrite(LED_BUILTIN, fLed);

    if (! gSdp.startTriggeredMeasurement())
        printFailure("gSdp.startTriggeredMeasurement failed");

    while (! gSdp.queryReady())
        {
        if (gSdp.getLastError() != cSDP::Error::Busy)
            printFailure("queryReady() failed");
        }

    if (! gSdp.readMeasurement())
        {
        printFailure("readMeasurement() failed");
        }

    auto m = gSdp.getMeasurement();

    Serial.print("T(F)=");
    Serial.print(m.Temperature * 1.8 + 32);
    Serial.print("  differential pressure=");
    Serial.print(m.DifferentialPressure);
    Serial.println(" Pa");

    delay(1000);
    }
