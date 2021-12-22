/*

Module: sdp-lorawan.ino

Function:
    Sensor sketch measuring and transmitting differential pressure info.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   September 2020

*/

#if defined(ARDUINO_MCCI_CATENA_4801) || defined(ARDUINO_MCCI_CATENA_4802)
/* Expected Architecture */
#else
# error "This sketch targets the MCCI Catena 4801/4802"
#endif

#include <Arduino.h>
#include "sdp_lorawan.h"
#include "cMeasurementLoop.h"
#include <arduino_lmic.h>

using namespace McciCatena;
using namespace McciCatenaSdp;


/****************************************************************************\
|
|   Constants.
|
\****************************************************************************/

constexpr std::uint32_t kAppVersion = makeVersion(1,1,1,0);

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

using namespace McciCatena;
using namespace McciCatenaSdp;

// the global Catena instance
Catena gCatena;
// the global LoRaWAN instance
Catena::LoRaWAN gLoRaWAN;
// the global LED instance
StatusLed gLed (Catena::PIN_STATUS_LED);

// the global secondary SPI bus (used for the flash)
SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

// the flash
Catena_Mx25v8035f gFlash;
// status flag, true if flash was probed at boot.
bool gfFlash;

// The SDP Sensor
cSDP gSDP { Wire, cSDP::Address::SDP8xx };

// the measurement loop instance
cMeasurementLoop gMeasurementLoop { gSDP };

// forward reference to the command functions
cCommandStream::CommandFn cmdDebugFlags;
cCommandStream::CommandFn cmdRunStop;

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
        {
        { "debugflags", cmdDebugFlags },
        { "run", cmdRunStop },
        { "stop", cmdRunStop },
        // other commands go here....
        };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyExtraCommands_top(
        sMyExtraCommmands,          /* this is the pointer to the table */
        sizeof(sMyExtraCommmands),  /* this is the size of the table */
        nullptr                     /* this is no "first word" for all the commands in this table */
        );

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    setup_platform();
    setup_printSignOn();

    setup_flash();
    setup_sensors();
    setup_radio();
    setup_commands();
    setup_measurement();
    }

void setup_platform()
    {
    /* power up sensor */
    pinMode(D11, OUTPUT);
    digitalWrite(D11, 1);

    /* power up other i2c sensors */
    pinMode(D10, OUTPUT);
    digitalWrite(D10, 1);

    gCatena.begin();

    gLed.begin();
    gCatena.registerObject(&gLed);
    gLed.Set(McciCatena::LedPattern::FastFlash);

    // if running unattended, don't wait for USB connect.
    if (! (gCatena.GetOperatingFlags() &
            static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)))
            {
            while (!Serial)
                    /* wait for USB attach */
                    yield();
            }

    gLed.Set(McciCatena::LedPattern::FiftyFiftySlow);

    // enable extended I2C.
    pinMode(D34, OUTPUT);
    digitalWrite(D34, 1);
    }

static constexpr const char *filebasename(const char *s)
    {
    const char *pName = s;

    for (auto p = s; *p != '\0'; ++p)
        {
        if (*p == '/' || *p == '\\')
            pName = p + 1;
        }
    return pName;
    }

void setup_printSignOn()
    {
    static const char dashes[] = "------------------------------------";

    gCatena.SafePrintf("\n%s%s\n", dashes, dashes);

    gCatena.SafePrintf("This is %s v%d.%d.%d.%d.\n",
        filebasename(__FILE__),
        getMajor(kAppVersion), getMinor(kAppVersion), getPatch(kAppVersion), getLocal(kAppVersion)
        );

    do  {
        char sRegion[16];
        gCatena.SafePrintf("Target network: %s / %s\n",
                        gLoRaWAN.GetNetworkName(),
                        gLoRaWAN.GetRegionString(sRegion, sizeof(sRegion))
                        );
        } while (0);

    gCatena.SafePrintf("System clock rate is %u.%03u MHz\n",
        ((unsigned)gCatena.GetSystemClockRate() / (1000*1000)),
        ((unsigned)gCatena.GetSystemClockRate() / 1000 % 1000)
        );
    gCatena.SafePrintf("Enter 'help' for a list of commands.\n");
    gCatena.SafePrintf("(remember to select 'Line Ending: Newline' at the bottom of the monitor window.)\n");

    gCatena.SafePrintf("%s%s\n" "\n", dashes, dashes);
    }

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gfFlash = true;
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gfFlash = false;
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    LMIC_setClockError(5 * MAX_CLOCK_ERROR / 100);
    }

void setup_sensors()
    {
    Wire.begin();
    if (! gSDP.begin())
        {
        gCatena.SafePrintf("gSDP.begin() failed %s(%u)\n",
                        gSDP.getLastErrorName(),
                        unsigned(gSDP.getLastError())
                        );
        }
    
    gMeasurementLoop.begin();
    }

void setup_commands()
    {
    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyExtraCommands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );
    }

void setup_measurement()
    {
    if (gLoRaWAN.IsProvisioned())
        gMeasurementLoop.requestActive(true);
    else
        {
        gCatena.SafePrintf("not provisioned, idling\n");
        gMeasurementLoop.requestActive(false);
        }
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();
    }

/****************************************************************************\
|
|   The commands -- called automatically from the framework after receiving
|   and parsing a command from the Serial console.
|
\****************************************************************************/

/* process "debugmask" -- args are ignored */
// argv[0] is the matched command name.
// argv[1] if present is the new mask
cCommandStream::CommandStatus cmdDebugFlags(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fResult;

        pThis->printf("%s\n", argv[0]);
        fResult = true;
        if (argc < 2)
            pThis->printf("debug mask: 0x%08x\n", gLog.getFlags());
        else if (argc > 2)
            {
            fResult = false;
            pThis->printf("too many args\n");
            }
        else
            {
            std::uint32_t newMask;
            bool fOverflow;
            size_t const nArg = std::strlen(argv[1]);

            if (nArg != McciAdkLib_BufferToUint32(
                                argv[1], nArg,
                                0,
                                &newMask, &fOverflow
                                ) || fOverflow)
                {
                pThis->printf("invalid mask: %s\n", argv[1]);
                fResult = false;
                }
            else
                {
                gLog.setFlags(cLog::DebugFlags(newMask));
                pThis->printf("mask is now 0x%08x\n", gLog.getFlags());
                }
            }

        return fResult ? cCommandStream::CommandStatus::kSuccess
                       : cCommandStream::CommandStatus::kInvalidParameter
                       ;
        }

/* process "run" or "stop" -- args are ignored */
// argv[0] is the matched command name.
cCommandStream::CommandStatus cmdRunStop(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        )
        {
        bool fEnable;

        pThis->printf("%s measurement loop\n", argv[0]);

        fEnable = argv[0][0] == 'r';
        gMeasurementLoop.requestActive(fEnable);
 
        return cCommandStream::CommandStatus::kSuccess;
        }
