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

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

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
    if (! gSdp.begin())
        {
        printFailure("gSdp.begin() failed");
        }
    }

void loop()
    {
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
