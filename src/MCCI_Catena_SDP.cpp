/*

Module:	MCCI_Catena_SDP.cpp

Function:
    Implementation code for MCCI Sensirion differential pressure sensor

Copyright and License:
    This file copyright (C) 2020 by

        MCCI Corporation
        3520 Krums Corners Road
        Ithaca, NY  14850

    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation	September 2020

*/

#include <MCCI_Catena_SDP.h>

using namespace McciCatenaSdp;

bool cSDP::begin()
    {
    // if no Wire is bound, fail.
    if (this->m_wire == nullptr)
        return this->setLastError(Error::NoWire);

    if (this->isRunning())
        return true;

    this->m_wire->begin();
    // the device might be asleep; assume nothing.
    this->m_state = State::Sleep;
    return this->readProductInfo();
    }

void cSDP::end()
    {
    if (this->isRunning())
        this->m_state = State::Uninitialized;
    }

bool cSDP::readProductInfo()
    {
    std::uint8_t productInfoRaw[6 * 3];

    if (! this->wakeup())
        return false;

    if (! this->writeCommand(Command::ReadProductId1))
        return false;

    if (! this->writeCommand(Command::ReadProductId2))
        return false;

    if (! this->readResponse(productInfoRaw, sizeof(productInfoRaw)))
        return false;

    if (! this->crc_multi(productInfoRaw, sizeof(productInfoRaw)))
        return false;

    // since we have the info, save it
    const std::uint32_t productNumber = (std::uint32_t(getUint16BE(&productInfoRaw[0]))  << 16)
                                      + (std::uint32_t(getUint16BE(&productInfoRaw[3]))  <<  0);
    const std::uint64_t serialNumber  = (std::uint64_t(getUint16BE(&productInfoRaw[6]))  << 48)
                                      + (std::uint64_t(getUint16BE(&productInfoRaw[9]))  << 32)
                                      + (std::uint64_t(getUint16BE(&productInfoRaw[12])) << 16)
                                      + (std::uint64_t(getUint16BE(&productInfoRaw[15])) <<  0)
                                      ;

    this->m_ProductInfo.ProductNumber = productNumber;
    this->m_ProductInfo.SerialNumber = serialNumber;

    return true;
    }

bool cSDP::wakeup()
    {
    if (this->m_state != State::Sleep)
        return true;

    // do a wakeup, and retry if it fails
    this->m_wire->beginTransmission(std::uint8_t(this->m_address));
    if (this->m_wire->endTransmission() != 0)
        {
        delay(2);
        this->m_wire->beginTransmission(std::uint8_t(this->m_address));
        
        if (this->m_wire->endTransmission() != 0)
            return this->setLastError(Error::WakeupFailed);
        }
    
    this->m_state = State::Idle;
    return true;
    }

bool cSDP::writeCommand(cSDP::Command command)
    {
    const std::uint8_t cbuf[2] = { std::uint8_t(std::uint16_t(command) >> 8), std::uint8_t(command) };

    this->m_wire->beginTransmission(std::uint8_t(this->m_address));
    if (this->m_wire->write(cbuf, sizeof(cbuf)) != sizeof(cbuf))
        return this->setLastError(Error::CommandWriteBufferFailed);
    if (this->m_wire->endTransmission() != 0)
        return this->setLastError(Error::CommandWriteFailed);

    return true;
    }

bool cSDP::readResponse(std::uint8_t *buf, size_t nBuf)
    {
    bool ok;
    unsigned nResult;
    const std::int8_t addr = this->getAddress();
    uint8_t nReadFrom;

    if (buf == nullptr || nBuf > 32 || addr < 0)
        {
        return this->setLastError(Error::InternalInvalidParameter);
        }

    nReadFrom = this->m_wire->requestFrom(std::uint8_t(addr), /* bytes */ std::uint8_t(nBuf));

    if (nReadFrom != nBuf)
        {
        return this->setLastError(Error::I2cReadRequest);
        }
    nResult = this->m_wire->available();
    if (nResult > nBuf)
        return this->setLastError(Error::I2cReadLong);

    for (unsigned i = 0; i < nResult; ++i)
        buf[i] = this->m_wire->read();

    if (nResult != nBuf)
        {
        return this->setLastError(Error::I2cReadShort);
        }

    return true;
    }

bool cSDP::startTriggeredMeasurement()
    {
    if (! this->wakeup())
        return false;

    if (this->m_state != State::Idle)
        return this->setLastError(Error::Busy);

    bool result = this->writeCommand(Command::StartTriggeredDifferential_Poll);

    if (result)
        {
        this->m_state = State::Triggered;
        this->m_tReady = millis() + 46;
        }

    return result;
    }

bool cSDP::queryReady()
    {
    if (! checkRunning())
        return false;

    if (this->m_state != State::Triggered)
        return this->setLastError(Error::NotMeasuring);
    
    if (std::int32_t(millis() - this->m_tReady) < 0)
        {
        return this->setLastError(Error::Busy);
        }

    return true;
    }

bool cSDP::readMeasurement()
    {
    if (! checkRunning())
        return false;

    if (this->m_state != State::Triggered)
        return this->setLastError(Error::NotMeasuring);

    std::uint8_t measurementBuffer[3 * 3];
    bool result = this->readResponse(measurementBuffer, sizeof(measurementBuffer));
    this->m_state = State::Idle;

    if (result)
        {
        result = this->crc_multi(measurementBuffer, sizeof(measurementBuffer));
        }

    if (result)
        {
        MeasurementRaw m;
        m.DifferentialPressureBits = getInt16BE(&measurementBuffer[0]);
        m.TemperatureBits = getInt16BE(&measurementBuffer[3]);
        m.ScaleBits = getUint16BE(&measurementBuffer[6]);

        this->m_MeasurementRaw = m;
        this->m_Measurement.set(m);
        }

    return result;
    }

std::uint8_t cSDP::crc(const std::uint8_t * buf, size_t nBuf, std::uint8_t crc8)
    {
    /* see cSHT3x CRC-8-Calc.md for a little info on this */
    static const std::uint8_t crcTable[16] =
        {
        0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97,
        0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c, 0x1f, 0x2e,
        };

    for (size_t i = nBuf; i > 0; --i, ++buf)
        {
        uint8_t b, p;

        // calculate first nibble
        b = *buf;
        p = (b ^ crc8) >> 4;
        crc8 = (crc8 << 4) ^ crcTable[p];

        // calculate second nibble
        // this could be written as:
        //      b <<= 4;
        //      p = (b ^ crc8) >> 4;
        // but it's more effective as:
        p = ((crc8 >> 4) ^ b) & 0xF;
        crc8 = (crc8 << 4) ^ crcTable[p];
        }

    return crc8;
    }

bool cSDP::crc_multi(const std::uint8_t *buf, size_t nbuf)
    {
    if (buf == nullptr)
        return this->setLastError(Error::InternalInvalidParameter);
    
    for (; nbuf >= 3; buf += 3, nbuf -= 3)
        {
        if (this->crc(buf, 2) != buf[2])
            return this->setLastError(Error::Crc);
        }

    if (nbuf != 0)
        return this->setLastError(Error::InternalInvalidParameter);

    return true;
    }

bool cSDP::sleep()
    {
    if (! checkRunning())
        return false;
    // no need to send commmand if asleep
    if (this->m_state == State::Sleep)
        return true;

    // can't send command if we're idle
    if (! (this->m_state == State::Idle))
        return this->setLastError(Error::Busy);

    bool const result = this->writeCommand(Command::EnterSleepMode);
    if (result)
        this->m_state = State::Sleep;

    return result;
    }

const char * cSDP::getErrorName(cSDP::Error e)
    {
    auto p = m_szErrorMessages;

    // iterate based on error index.
    for (unsigned eIndex = unsigned(e); eIndex > 0; --eIndex)
        {
        // stop when we get to empty string
        if (*p == '\0')
            break;
        // otherwise, skip this one.
        p += strlen(p) + 1;
        }

    // if we have a valid string, return it
    if (*p != '\0')
        return p;
    // otherwise indicate that the input wasn't valid.
    else
        return "<<unknown>>";
    }
