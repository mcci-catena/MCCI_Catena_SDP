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
#include <cstdint>

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
        Serial.print(gSdp.getLastErrorName());
        Serial.print("(");
        Serial.print(std::uint8_t(gSdp.getLastError()));
        Serial.print(")");
        delay(2000);
        }
    }

// format a uint64_t into a buffer and return pointer to first byte.
char *formatUint64(char *pBuf, size_t nBuf, std::uint64_t v, unsigned base)
    {
    char *p;

    if (pBuf == nullptr || nBuf == 0)
        return nullptr;

    p = pBuf + nBuf;
    *--p = '\0';
    while (p > pBuf)
        {
        unsigned digit;
        
        digit = unsigned(v % base);
        v = v / base;
        --p;

        if (digit < 10)
            *p = '0' + digit;
        else
            *p = 'A' + digit - 10;

        if (v == 0)
            break;
        }

    if (p == pBuf && v != 0)
        *p = '*';

    return p;
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

    Serial.print("Found sensor ");
    Serial.print(gSdp.getProductName());
    Serial.print(", serial number ");

    // serial number is uint64. Max is 20 digits plus space. 
    char snbuffer[21];

    Serial.println(formatUint64(snbuffer, sizeof(snbuffer), gSdp.getSerialNumber(), 10));
    }

void loop()
    {
    // toggle the LED
    fLed = !fLed;
    digitalWrite(LED_BUILTIN, fLed);

    // start a measurement
    if (! gSdp.startTriggeredMeasurement())
        printFailure("gSdp.startTriggeredMeasurement failed");

    // wait for measurement to complete.
    while (! gSdp.queryReady())
        {
        if (gSdp.getLastError() != cSDP::Error::Busy)
            printFailure("queryReady() failed");
        }

    // get the results.
    if (! gSdp.readMeasurement())
        {
        printFailure("readMeasurement() failed");
        }

    // put the sensor to sleep.
    if (! gSdp.sleep())
        {
        printFailure("sleep() failed");
        }

    // display the results by fetching then displaying.
    auto m = gSdp.getMeasurement();

    Serial.print("T(F)=");
    Serial.print(m.Temperature * 1.8 + 32);
    Serial.print("  differential pressure=");
    Serial.print(m.DifferentialPressure);
    Serial.println(" Pa");

    delay(1000);
    }
