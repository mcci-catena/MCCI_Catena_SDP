/*

Module: MCCI_Catena_SDP.h

Function:
    Definitions for the Catena library for the Sensirion SDP sensor family.

Copyright and License:
    See accompanying LICENSE file.

Author:
    Terry Moore, MCCI Corporation   September 2020

*/

#ifndef _MCCI_CATENA_SDP_H_
# define _MCCI_CATENA_SDP_H_
# pragma once

#include <cstdint>
#include <Wire.h>

namespace McciCatenaSdp {

// create a version number for comparison
static constexpr std::uint32_t
makeVersion(
    std::uint8_t major, std::uint8_t minor, std::uint8_t patch, std::uint8_t local = 0
    )
    {
    return ((std::uint32_t)major << 24) | ((std::uint32_t)minor << 16) | ((std::uint32_t)patch << 8) | (std::uint32_t)local;
    }

// extract major number from version
static constexpr std::uint8_t
getMajor(std::uint32_t v)
    {
    return std::uint8_t(v >> 24u);
    }

// extract minor number from version
static constexpr std::uint8_t
getMinor(std::uint32_t v)
    {
    return std::uint8_t(v >> 16u);
    }

// extract patch number from version
static constexpr std::uint8_t
getPatch(std::uint32_t v)
    {
    return std::uint8_t(v >> 8u);
    }

// extract local number from version
static constexpr std::uint8_t
getLocal(std::uint32_t v)
    {
    return std::uint8_t(v);
    }

// version of library, for use by clients in static_asserts
static constexpr std::uint32_t kVersion = makeVersion(0,0,1,0);

class cSDP
    {
private:
    static constexpr bool kfDebug = false;

public:
    // the address type:
    enum class Address : std::int8_t
        {
        Error = -1,
        SDP3x_A = 0x21,
        SDP3x_B = 0x22,
        SDP3x_C = 0x23,
        SDP8xx = 0x25,
        };

    // the type for pin assignments, in case it's an SDP3x and has an int pin.
    using Pin_t = std::int8_t;

    // constructor
    cSDP(TwoWire &wire, Address Address = Address::SDP3x_A, Pin_t pinAlert = -1)
        : m_wire(&wire)
        , m_address(Address)
        , m_pinAlert(pinAlert)
        {}

    // neither copyable nor movable
    cSDP(const cSDP&) = delete;
    cSDP& operator=(const cSDP&) = delete;
    cSDP(const cSDP&&) = delete;
    cSDP& operator=(const cSDP&&) = delete;

    static constexpr float rawTtoCelsius(std::int16_t tfrac)
        {
        return float(tfrac) / 200.0f;
        }

    static constexpr float rawDiffPtoPascal(std::int16_t diffP, std::uint16_t scale)
        {
        return float(diffP) / float(scale);
        }

    // raw measurement as a collection.
    struct MeasurementRaw
        {
        std::int16_t   TemperatureBits;
        std::int16_t   DifferentialPressureBits;
        std::uint16_t   ScaleBits;
        void extract(std::int16_t &a_t, std::int16_t &a_dp, std::uint16_t &a_scale) const
            {
            a_t = this->TemperatureBits;
            a_dp = this->DifferentialPressureBits;
            a_scale = this->ScaleBits;
            }
        };

    // measurements, as a collection.
    struct Measurement
        {
        float Temperature;
        float DifferentialPressure;
        void set(MeasurementRaw const & mRaw)
            {
            this->Temperature = rawTtoCelsius(mRaw.TemperatureBits);
            this->DifferentialPressure = rawDiffPtoPascal(mRaw.DifferentialPressureBits, mRaw.ScaleBits);
            }
        void extract(float &a_t, float &a_dp) const
            {
            a_t = this->Temperature;
            a_dp = this->DifferentialPressure;
            }
        };

    // product ID info
    struct ProductInfo
        {
        std::uint64_t   SerialNumber;
        std::uint32_t   ProductNumber;
        };

    enum class ProductId_t : std::uint32_t
        {
        SDP31       = 0x03010101,
        SDP32       = 0x03010102,
        SDP800_500  = 0x03020101,
        SDP810_500  = 0x03020A01,
        SDP801_500  = 0x03020401,
        SDP811_500  = 0x03020D01,
        SDP800_125  = 0x03020201,
        SDP810_125  = 0x03020B01,
        };
    
    static constexpr const char * getProductName(ProductId_t id)
        {
        return (id == ProductId_t::SDP31)       ? "SDP31"
            :  (id == ProductId_t::SDP32)       ? "SDP32"
            :  (id == ProductId_t::SDP800_500)  ? "SDP800-500Pa"
            :  (id == ProductId_t::SDP810_500)  ? "SDP810-500Pa"
            :  (id == ProductId_t::SDP801_500)  ? "SDP801-500Pa"
            :  (id == ProductId_t::SDP811_500)  ? "SDP811-500Pa"
            :  (id == ProductId_t::SDP800_125)  ? "SDP800-125Pa"
            :  (id == ProductId_t::SDP810_125)  ? "SDP810-125Pa"
            :  "<<unknown>>"
            ;
        }

    // the commands
    enum class Command : std::uint16_t
        {
        // sorted in ascending numerical order.
        StartContinuousMassFlow_Average         =   0x3603,
        StartContinuousMassFlow_Point           =   0x3608,
        StartContinuousDifferential_Average     =   0x3615,
        StartContinuousDifferential_Point       =   0x361E,
        StartTriggeredMassflow_Poll             =   0x3624,
        StartTriggeredDifferential_Poll         =   0x362F,
        EnterSleepMode                          =   0x3677,
        ReadProductId1                          =   0x367C,
        StartTriggeredMassflow_Stretch          =   0x3726,
        StartTriggeredDifferential_Stretch      =   0x372D,
        ReadProductId2                          =   0xE102,
        };

    // the errors
    enum class Error : std::uint8_t
        {
        Success = 0,
        NoWire,
        CommandWriteFailed,
        CommandWriteBufferFailed,
        InternalInvalidParameter,
        I2cReadShort,
        I2cReadRequest,
        I2cReadLong,
        WakeupFailed,
        Busy,
        NotMeasuring,
        Crc,
        Uninitialized,
        };

private:
    // this is internal -- centralize it but require that clients call the 
    // public method (which centralizes the strings and the search)
    static constexpr const char *m_szErrorMessages = 
        "Success\0"
        "NoWire\0"
        "CommandWriteFailed\0"
        "CommandWriteBufferFailed\0"
        "InternalInvalidParameter\0"
        "I2cReadShort\0"
        "I2cReadRequest\0"
        "I2cReadLong\0"
        "WakeupFailed\0"
        "Busy\0"
        "NotMeasuring\0"
        "Crc\0"
        "Uninitialized\0"
        ;

public:
    // the public methods
    bool begin();
    void end();
    bool startTriggeredMeasurement();
    bool queryReady();
    bool readMeasurement();
    float getTemperature() const { return this->m_Measurement.Temperature; }
    float getDifferentialPressure() const { return this->m_Measurement.DifferentialPressure; }
    Measurement getMeasurement() const { return this->m_Measurement; }
    MeasurementRaw getRawMeasurement() const { return this->m_MeasurementRaw; }
    Error getLastError() const
        {
        return this->m_lastError;
        }
    bool setLastError(Error e)
        {
        this->m_lastError = e;
        return e == Error::Success;
        }
    static const char *getErrorName(Error e);
    const char *getLastErrorName() const
        {
        return getErrorName(this->m_lastError);
        }
    const char *getProductName() const
        {
        return getProductName(ProductId_t(this->m_ProductInfo.ProductNumber));
        }
    bool readProductInfo();
    bool sleep();
    bool isRunning() const
        {
        return this->m_state != State::Uninitialized;
        }
    enum class State : std::uint8_t
        {
        Uninitialized,
        Idle,
        Triggered,
        Continuous,
        Sleep,
        };

protected:
    bool writeCommand(Command c);
    bool readResponse(std::uint8_t *buf, size_t nBuf);
    static std::uint8_t crc(const std::uint8_t *buf, size_t nBuf, std::uint8_t crc8 = 0xFF);
    bool crc_multi(const std::uint8_t *buf, size_t nBuf);
    std::int8_t getAddress() const
        { return static_cast<std::int8_t>(this->m_address); }
    bool wakeup();
    bool checkRunning() 
        {
        if (! this->isRunning())
            return this->setLastError(Error::Uninitialized);
        else
            return true;
        }

// following are arranged for alignment.
private:
    Measurement m_Measurement;      /// most recent measurement
    TwoWire *m_wire;                /// pointer to bus to be used for this device
    std::uint32_t m_tReady;         /// time next measurement will be ready (millis)
    ProductInfo m_ProductInfo;      /// product information read from device
    MeasurementRaw m_MeasurementRaw; /// most recent raw data
    Address m_address;              /// I2C address to be used
    Pin_t m_pinAlert;               /// alert pin, or -1 if none.
    Error m_lastError;              /// last error.
    State m_state                   /// current state
        { State::Uninitialized };   // initially not yet started.

    static constexpr std::uint16_t getUint16BE(const std::uint8_t *p)
        {
        return (p[0] << 8) + p[1];
        }
     static constexpr std::int16_t getInt16BE(const std::uint8_t *p)
        {
        return std::int16_t((p[0] << 8) + p[1]);
        }
   };

} // namespace McciCatenaSdp

#endif // _MCCI_CATENA_SDP_H_
