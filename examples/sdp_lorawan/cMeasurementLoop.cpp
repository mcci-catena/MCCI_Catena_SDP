/*

Module: cMeasurementLoop.cpp

Function:
    Sensor sketch measuring and transmitting differential air pressure info.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   Sept 2020

*/

#include "cMeasurementLoop.h"

#include "sdp_lorawan.h"
#include <arduino_lmic.h>

#ifndef ARDUINO_MCCI_CATENA_4801
# error "This sketch targets the MCCI Catena 4801"
#endif

using namespace McciCatena;
using namespace McciCatenaSdp;

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

void cMeasurementLoop::begin()
    {
    // turn on flags for debugging.
    // gLog.setFlags(cLog::DebugFlags(gLog.getFlags() | gLog.kTrace | gLog.kInfo));

    // assume we have a pressure sensor
    this->m_fDiffPressure = true;

    // register for polling.
    if (! this->m_registered)
        {
        this->m_registered = true;

        gCatena.registerObject(this);

        this->m_UplinkTimer.begin(this->m_txCycleSec * 1000);
        }

    if (! this->m_running)
        {
        this->m_exit = false;
        this->m_fsm.init(*this, &cMeasurementLoop::fsmDispatch);
        }
    }

void cMeasurementLoop::end()
    {
    if (this->m_running)
        {
        this->m_exit = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::requestActive(bool fEnable)
    {
    if (fEnable)
        this->m_rqActive = true;
    else
        this->m_rqInactive = true;

    this->m_fsm.eval();
    }

cMeasurementLoop::State cMeasurementLoop::fsmDispatch(
    cMeasurementLoop::State currentState,
    bool fEntry
    )
    {
    State newState = State::stNoChange;

    if (fEntry && gLog.isEnabled(gLog.DebugFlags::kTrace))
        {
        gLog.printf(
                gLog.DebugFlags::kAlways,
                "cMeasurementLoop::fsmDispatch: enter %s\n",
                this->getStateName(currentState)
                );
        }

    switch (currentState)
        {
    case State::stInitial:
        newState = State::stInactive;
        break;

    case State::stInactive:
        if (fEntry)
            {
            if (! this->m_Sdp.sleep())
                {
                if (gLog.isEnabled(gLog.DebugFlags::kError))
                    gLog.printf(
                        gLog.kAlways,
                        "SDP sleep failed: status %s(%u)\n",
                        this->m_Sdp.getLastErrorName(),
                        unsigned(this->m_Sdp.getLastError())
                        );
                }
            }
        if (this->m_rqActive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = true;
            this->m_UplinkTimer.retrigger();
            newState = State::stWake;
            }
        break;

    case State::stSleeping:
        if (fEntry)
            {
            if (! this->m_Sdp.sleep())
                {
                if (gLog.isEnabled(gLog.DebugFlags::kError))
                    gLog.printf(
                        gLog.kAlways,
                        "SDP sleep failed: status %s(%u)\n",
                        this->m_Sdp.getLastErrorName(),
                        unsigned(this->m_Sdp.getLastError())
                        );
                }
            gLed.Set(McciCatena::LedPattern::Sleeping);
            }

        if (this->m_rqInactive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = false;
            newState = State::stInactive;
            }
        else if (this->m_UplinkTimer.isready())
            newState = State::stWake;
        else if (this->m_UplinkTimer.getRemaining() > 1500)
            this->sleep();
        break;

    // in this state, do anything needed after sleep.
    case State::stWake:
        if (fEntry)
            {
            this->setTimer(20);
            }
        if (this->timedOut())
            {
            newState = State::stMeasure;
            }
        break;

    case State::stMeasure:
        if (fEntry)
            {
            this->setTimer(2 * 1000);
            this->m_measurement_valid = false;
            if (! this->m_Sdp.startTriggeredMeasurement())
                {
                if (gLog.isEnabled(gLog.DebugFlags::kError))
                    gLog.printf(
                        gLog.kAlways,
                        "SDP startTriggeredMeasurement failed: status %s(%u)\n",
                        this->m_Sdp.getLastErrorName(),
                        unsigned(this->m_Sdp.getLastError())
                        );
                }
            }
        if (this->m_Sdp.queryReady())
            {
            this->clearTimer();
            this->m_measurement_valid = this->m_Sdp.readMeasurement();
            if ((! this->m_measurement_valid) && gLog.isEnabled(gLog.kError))
                {
                gLog.printf(gLog.kAlways, "SDP measurement failed: error %s(%u)\n",
                        this->m_Sdp.getLastErrorName(),
                        unsigned(this->m_Sdp.getLastError())
                    );
                }
            newState = State::stSleepSensor;
            }
        else if (this->m_Sdp.getLastError() != cSDP::Error::Busy)
            {
            if (gLog.isEnabled(gLog.DebugFlags::kError))
                gLog.printf(
                    gLog.kAlways,
                    "SDP queryReady failed: status %s(%u)\n",
                    this->m_Sdp.getLastErrorName(),
                    unsigned(this->m_Sdp.getLastError())
                    );

            newState = State::stSleepSensor;
            }
        else if (this->timedOut())
            {
            if (gLog.isEnabled(gLog.kError))
                {
                gLog.printf(gLog.kAlways, "SDP measurement timed out\n");
                }
    
            newState = State::stSleepSensor;
            }
        break;

    case State::stSleepSensor:
        if (fEntry)
            {
            if (! this->m_Sdp.sleep())
                {
                if (gLog.isEnabled(gLog.DebugFlags::kError))
                    gLog.printf(
                        gLog.kAlways,
                        "SDP sleep failed: status %s(%u)\n",
                        this->m_Sdp.getLastErrorName(),
                        unsigned(this->m_Sdp.getLastError())
                        );
                }
            }

        newState = State::stTransmit;
        break;

    case State::stTransmit:
        if (fEntry)
            {
            TxBuffer_t b;
            this->fillTxBuffer(b);
            this->startTransmission(b);
            }
        if (this->txComplete())
            {
            newState = State::stSleeping;

            // calculate the new sleep interval.
            this->updateTxCycleTime();
            }
        break;

    case State::stFinal:
        break;

    default:
        break;
        }
    
    return newState;
    }

/****************************************************************************\
|
|   Prepare a buffer to be transmitted.
|
\****************************************************************************/

void cMeasurementLoop::fillTxBuffer(cMeasurementLoop::TxBuffer_t& b)
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Measuring);

    b.begin();
    Flags flag;

    flag = Flags(0);

    // insert format byte
    b.put(kMessageFormat);

    // insert a byte that will become flags later.
    std::uint8_t * const pFlag = b.getp();
    b.put(std::uint8_t(flag));

    // send Vbat
    float Vbat = gCatena.ReadVbat();
    gCatena.SafePrintf("Vbat:    %d mV\n", (int) (Vbat * 1000.0f));
    b.putV(Vbat);
    flag |= Flags::Vbat;

    // send Vdd if we can measure it.

    // send boot count
    uint32_t bootCount;
    if (gCatena.getBootCount(bootCount))
        {
        b.putBootCountLsb(bootCount);
        flag |= Flags::Boot;
        }

    if (this->m_fDiffPressure && this->m_measurement_valid)
        {
        auto const mraw = this->m_Sdp.getRawMeasurement();
        auto const m = this->m_Sdp.getMeasurement();

        // temperature is 2 bytes from -163.840 to +163.835 degrees C
        // pressure is 4 bytes, first signed units, then scale.
        if (gLog.isEnabled(gLog.kInfo))
            {
            char ts = ' ';
            std::int32_t t100 = std::int32_t(m.Temperature * 100.0f + 0.5f);
            if (m.Temperature < 0) { ts = '-'; t100 = -t100; }
            std::int32_t tint = t100 / 100;
            std::int32_t tfrac = t100 - (tint * 100);

            char dps = '+';
            std::int32_t dp100 = std::int32_t(m.DifferentialPressure * 100.0f + 0.5f);
            if (m.DifferentialPressure < 0) { dps = '-'; dp100 = -dp100; }
            std::int32_t dpint = dp100 / 100;
            std::int32_t dpfrac = dp100 - (dpint * 100);

            gCatena.SafePrintf(
                "SDP:  T: %c%d.%02d  delta-P: %c%d.%0d2\n",
                ts, tint, tfrac,
                dps, dpint, dpfrac
                );
            }

        b.put2(std::uint32_t(mraw.TemperatureBits));
        // put2 takes a uint32_t or int32_t. We want the uint32_t version,
        // so we cast. f2sflt16() takes (-1.0f, +1.0f) and returns a uint16_t
        // as an encoding.
        b.put2(std::uint32_t(LMIC_f2sflt16(m.DifferentialPressure * 60.0f / 32768.0f)));

        flag |= Flags::DP | Flags::T;
        }

    *pFlag = std::uint8_t(flag);

    gLed.Set(savedLed);
    }

/****************************************************************************\
|
|   Reduce a single data set
|
\****************************************************************************/

/****************************************************************************\
|
|   Start uplink of data
|
\****************************************************************************/

void cMeasurementLoop::startTransmission(
    cMeasurementLoop::TxBuffer_t &b
    )
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Sending);

    // by using a lambda, we can access the private contents
    auto sendBufferDoneCb =
        [](void *pClientData, bool fSuccess)
            {
            auto const pThis = (cMeasurementLoop *)pClientData;
            pThis->m_txpending = false;
            pThis->m_txcomplete = true;
            pThis->m_txerr = ! fSuccess;
            pThis->m_fsm.eval();
            };

    bool fConfirmed = false;
    if (gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fConfirmedUplink))
        {
        gCatena.SafePrintf("requesting confirmed tx\n");
        fConfirmed = true;
        }

    this->m_txpending = true;
    this->m_txcomplete = this->m_txerr = false;

    if (! gLoRaWAN.SendBuffer(b.getbase(), b.getn(), sendBufferDoneCb, (void *)this, fConfirmed, kUplinkPort))
        {
        // uplink wasn't launched.
        this->m_txcomplete = true;
        this->m_txerr = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::sendBufferDone(bool fSuccess)
    {
    this->m_txpending = false;
    this->m_txcomplete = true;
    this->m_txerr = ! fSuccess;
    this->m_fsm.eval();
    }

/****************************************************************************\
|
|   The Polling function -- 
|
\****************************************************************************/

void cMeasurementLoop::poll()
    {
    bool fEvent;

    // no need to evaluate unless something happens.
    fEvent = false;

    // if we're not active, and no request, nothing to do.
    if (! this->m_active)
        {
        if (! this->m_rqActive)
            return;

        // we're asked to go active. We'll want to eval.
        fEvent = true;
        }

    if (this->m_fTimerActive)
        {
        if ((millis() - this->m_timer_start) >= this->m_timer_delay)
            {
            this->m_fTimerActive = false;
            this->m_fTimerEvent = true;
            fEvent = true;
            }
        }

    // check the transmit time.
    if (this->m_UplinkTimer.peekTicks() != 0)
        {
        fEvent = true;
        }

    if (fEvent)
        this->m_fsm.eval();
    }

/****************************************************************************\
|
|   Update the TxCycle count. 
|
\****************************************************************************/

void cMeasurementLoop::updateTxCycleTime()
    {
    auto txCycleCount = this->m_txCycleCount;

    // update the sleep parameters
    if (txCycleCount > 1)
            {
            // values greater than one are decremented and ultimately reset to default.
            this->m_txCycleCount = txCycleCount - 1;
            }
    else if (txCycleCount == 1)
            {
            // it's now one (otherwise we couldn't be here.)
            gCatena.SafePrintf("resetting tx cycle to default: %u\n", this->m_txCycleSec_Permanent);

            this->setTxCycleTime(this->m_txCycleSec_Permanent, 0);
            }
    else
            {
            // it's zero. Leave it alone.
            }
    }

/****************************************************************************\
|
|   Handle sleep between measurements 
|
\****************************************************************************/

void cMeasurementLoop::sleep()
    {
    const bool fDeepSleep = checkDeepSleep();

    if (! this->m_fPrintedSleeping)
            this->doSleepAlert(fDeepSleep);

    if (fDeepSleep)
            this->doDeepSleep();
    }

bool cMeasurementLoop::checkDeepSleep()
    {
    bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    bool fDeepSleep;
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval < 2)
            fDeepSleep = false;
    else if (fDeepSleepTest)
            {
            fDeepSleep = true;
            }
#ifdef USBCON
    else if (Serial.dtr())
            {
            fDeepSleep = false;
            }
#endif
    else if (gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDisableDeepSleep))
            {
            fDeepSleep = false;
            }
    else if ((gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)) != 0)
            {
            fDeepSleep = true;
            }
    else
            {
            fDeepSleep = false;
            }

    return fDeepSleep;
    }

void cMeasurementLoop::doSleepAlert(bool fDeepSleep)
    {
    this->m_fPrintedSleeping = true;

    if (fDeepSleep)
        {
        bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
        const uint32_t deepSleepDelay = fDeepSleepTest ? 10 : 30;

        gCatena.SafePrintf("using deep sleep in %u secs"
#ifdef USBCON
                            " (USB will disconnect while asleep)"
#endif
                            ": ",
                            deepSleepDelay
                            );

        // sleep and print
        gLed.Set(McciCatena::LedPattern::TwoShort);

        for (auto n = deepSleepDelay; n > 0; --n)
            {
            uint32_t tNow = millis();

            while (uint32_t(millis() - tNow) < 1000)
                {
                gCatena.poll();
                yield();
                }
            gCatena.SafePrintf(".");
            }
        gCatena.SafePrintf("\nStarting deep sleep.\n");
        uint32_t tNow = millis();
        while (uint32_t(millis() - tNow) < 100)
            {
            gCatena.poll();
            yield();
            }
        }
    else
        gCatena.SafePrintf("using light sleep\n");
    }

void cMeasurementLoop::doDeepSleep()
    {
    // bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
    //                         static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval == 0)
        return;

    /* ok... now it's time for a deep sleep */
    gLed.Set(McciCatena::LedPattern::Off);
    this->deepSleepPrepare();

    /* sleep */
    gCatena.Sleep(sleepInterval);

    /* recover from sleep */
    this->deepSleepRecovery();

    /* and now... we're awake again. trigger another measurement */
    this->m_fsm.eval();
    }

void cMeasurementLoop::deepSleepPrepare(void)
    {
    // this code is very specific to the MCCI Catena 4801 and
    // depends on the BSP's .end() methods to really shut things
    // down. If porting, bear this in mind; you'll need to modify
    // this.
    this->m_Sdp.end();
    Serial.end();
    Wire.end();
    SPI.end();
    if (gfFlash)
        gSPI2.end();
    digitalWrite(D10, 0);
    pinMode(D10, INPUT);    // this reduces power!
    digitalWrite(D11, 0);
    pinMode(D11, INPUT);    // this also reduces power!
    // in case power boost was on, turn it off.
    pinMode(D5, INPUT);
    }

void cMeasurementLoop::deepSleepRecovery(void)
    {
    // this code is very specific to the MCCI Catena 4801 and
    // reverses the work done by deepSleepPrepare(). If porting, bear
    // this in mind; you'll need to modify this routine.
    pinMode(D11, OUTPUT);
    digitalWrite(D11, 1);
    pinMode(D10, OUTPUT);
    digitalWrite(D10, 1);
    Serial.begin();
    Wire.begin();
    SPI.begin();
    if (gfFlash)
            gSPI2.begin();

    this->m_fDiffPressure = this->m_Sdp.begin();
    if (! this->m_fDiffPressure)
        {
        if (gLog.isEnabled(gLog.DebugFlags::kError))
            gLog.printf(
                gLog.kAlways,
                "SDP begin() failed after sleep: status %s(%u)\n",
                this->m_Sdp.getLastErrorName(),
                unsigned(this->m_Sdp.getLastError())
                );
        }
    }
