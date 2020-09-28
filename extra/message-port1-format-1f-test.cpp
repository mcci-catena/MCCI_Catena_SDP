/*

Module:	message-port1-format-1f-test.cpp

Function:
	Test vector generator for port 1, format 0x1f

Copyright and License:
	This file copyright (C) 2020 by

		MCCI Corporation
		3520 Krums Corners Road
		Ithaca, NY  14850

	See accompanying LICENSE file for copyright and license information.

Author:
	Terry Moore, MCCI Corporation	September 2020

*/

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

std::string key;
std::string value;

template <typename T>
struct val
    {
    bool fValid;
    T v;
    };

struct Measurements
    {
    val<float> Vbat;
    val<float> Vsys;
    val<float> Vbus;
    val<std::uint8_t> Boot;
    val<float> Temperature;
    val<float> DifferentialPressure;
    };

uint16_t
LMIC_f2uflt16(
        float f
        )
        {
        if (f < 0.0)
                return 0;
        else if (f >= 1.0)
                return 0xFFFF;
        else
                {
                int iExp;
                float normalValue;

                normalValue = std::frexp(f, &iExp);

                // f is supposed to be in [0..1), so useful exp
                // is [0..-15]
                iExp += 15;
                if (iExp < 0)
                        // underflow.
                        iExp = 0;

                // bits 15..12 are the exponent
                // bits 11..0 are the fraction
                // we conmpute the fraction and then decide if we need to round.
                uint16_t outputFraction = std::ldexp(normalValue, 12) + 0.5;
                if (outputFraction >= (1 << 12u))
                        {
                        // reduce output fraction
                        outputFraction = 1 << 11;
                        // increase exponent
                        ++iExp;
                        }

                // check for overflow and return max instead.
                if (iExp > 15)
                        return 0xFFFF;

                return (uint16_t)((iExp << 12u) | outputFraction);
                }
        }

uint16_t
LMIC_f2sflt16(
        float f
        )
        {
        if (f <= -1.0)
                return 0xFFFF;
        else if (f >= 1.0)
                return 0x7FFF;
        else
                {
                int iExp;
                float normalValue;
                uint16_t sign;

                normalValue = frexpf(f, &iExp);

                sign = 0;
                if (normalValue < 0)
                        {
                        // set the "sign bit" of the result
                        // and work with the absolute value of normalValue.
                        sign = 0x8000;
                        normalValue = -normalValue;
                        }

                // abs(f) is supposed to be in [0..1), so useful exp
                // is [0..-15]
                iExp += 15;
                if (iExp < 0)
                        iExp = 0;

                // bit 15 is the sign
                // bits 14..11 are the exponent
                // bits 10..0 are the fraction
                // we conmpute the fraction and then decide if we need to round.
                uint16_t outputFraction = ldexpf(normalValue, 11) + 0.5;
                if (outputFraction >= (1 << 11u))
                        {
                        // reduce output fraction
                        outputFraction = 1 << 10;
                        // increase exponent
                        ++iExp;
                        }

                // check for overflow and return max instead.
                if (iExp > 15)
                        return 0x7FFF | sign;

                return (uint16_t)(sign | (iExp << 11u) | outputFraction);
                }
        }

std::uint16_t encode16s(float v)
    {
    float nv = std::floor(v + 0.5f);

    if (nv > 32767.0f)
        return 0x7FFFu;
    else if (nv < -32768.0f)
        return 0x8000u;
    else
        {
        return (std::uint16_t) std::int16_t(nv);
        }
    }

std::uint16_t encode16u(float v)
    {
    float nv = std::floor(v + 0.5f);
    if (nv > 65535.0f)
        return 0xFFFFu;
    else if (nv < 0.0f)
        return 0;
    else
        {
        return std::uint16_t(nv);
        }
    }

std::uint16_t encodeV(float v)
    {
    return encode16s(v * 4096.0f);
    }

std::uint16_t encodeT(float v)
    {
    return encode16s(v * 200.0f);
    }

std::uint16_t encodeDiffP(float v)
    {
    return encode16u(LMIC_f2sflt16(v * 60.0f / 32768.0f));
    }

class Buffer : public std::vector<std::uint8_t>
    {
public:
    Buffer() : std::vector<std::uint8_t>() {};

    void push_back_be(std::uint16_t v)
        {
        this->push_back(std::uint8_t(v >> 8));
        this->push_back(std::uint8_t(v & 0xFF));
        }
    };

void encodeMeasurement(Buffer &buf, Measurements &m)
    {
    std::uint8_t flags = 0;

    // sent the type byte
    buf.clear();
    buf.push_back(0x1F);
    buf.push_back(0u); // flag byte.

    // put the fields
    if (m.Vbat.fValid)
        {
        flags |= 1 << 0;
        buf.push_back_be(encodeV(m.Vbat.v));
        }

    if (m.Vsys.fValid)
        {
        flags |= 1 << 1;
        buf.push_back_be(encodeV(m.Vsys.v));
        }

    if (m.Boot.fValid)
        {
        flags |= 1 << 2;
        buf.push_back(m.Boot.v);
        }
    
    if (m.Temperature.fValid)
        {
        flags |= 1 << 3;

        buf.push_back_be(encodeT(m.Temperature.v));
        }

    if (m.DifferentialPressure.fValid)
        {
        flags |= 1 << 4;

        buf.push_back_be(encodeDiffP(m.DifferentialPressure.v));
        }

    // update the flags
    buf.data()[1] = flags;
    }

void logMeasurement(Measurements &m)
    {
    class Padder {
    public:
        Padder() : m_first(true) {}
        const char *get() {
            if (this->m_first)
                {
                this->m_first = false;
                return "";
                }
            else
                return " ";
            }
        const char *nl() {
            return this->m_first ? "" : "\n";
            }
    private:
        bool m_first;
    } pad;

    std::cout << std::dec;

    // put the fields
    if (m.Vbat.fValid)
        {
        std::cout << pad.get() << "Vbat " << m.Vbat.v;
        }

    if (m.Vsys.fValid)
        {
        std::cout << pad.get() << "Vsys " << m.Vsys.v;
        }

    if (m.Boot.fValid)
        {
        std::cout << pad.get() << "Boot " << unsigned(m.Boot.v);
        }
    
    if (m.Temperature.fValid)
        {
        std::cout << pad.get() << "T " << m.Temperature.v;
        }

    if (m.DifferentialPressure.fValid)
        {
        std::cout << pad.get() << "deltaP " << m.DifferentialPressure.v;
        }

    // make the syntax cut/pastable.
    std::cout << pad.get() << ".\n";
    }

void putTestVector(Measurements &m)
    {
    Buffer buf {};
    logMeasurement(m);
    encodeMeasurement(buf, m);
    bool fFirst;

    fFirst = true;
    for (auto v : buf)
        {
        if (! fFirst)
            std::cout << " ";
        fFirst = false;
        std::cout.width(2);
        std::cout.fill('0');
        std::cout << std::hex << unsigned(v);
        }
    std::cout << "\n";
    }

int main(int argc, char **argv)
    {
    Measurements m {0};
    Measurements m0 {0};
    bool fAny;

    std::cout << "Input a line with name/values pairs\n";

    fAny = false;
    while (std::cin.good())
        {
        bool fUpdate = true;
        key.clear();

        std::cin >> key;

        if (key == "Vbat")
            {
            std::cin >> m.Vbat.v;
            m.Vbat.fValid = true;
            }
        else if (key == "Vsys")
            {
            std::cin >> m.Vsys.v;
            m.Vsys.fValid = true;
            }
        else if (key == "Boot")
            {
            std::uint32_t nonce;
            std::cin >> nonce;
            m.Boot.v = (std::uint8_t) nonce;
            m.Boot.fValid = true;
            }
        else if (key == "T")
            {
            std::cin >> m.Temperature.v;
            m.Temperature.fValid = true;
            }
        else if (key == "deltaP")
            {
            std::cin >> m.DifferentialPressure.v;
            m.DifferentialPressure.fValid = true;
            }
        else if (key == ".")
            {
            putTestVector(m);
            m = m0;
            fAny = false;
            fUpdate = false;
            }
        else if (key == "")
            /* ignore empty keys */
            fUpdate = false;
        else
            {
            std::cerr << "unknown key: " << key << "\n";
            fUpdate = false;
            }

        fAny |= fUpdate;
        }
    
    if (!std::cin.eof() && std::cin.fail())
        {
        std::string nextword;

        std::cin.clear(std::cin.goodbit);
        std::cin >> nextword;
        std::cerr << "parse error: " << nextword << "\n";
        return 1;
        }

    if (fAny)
        putTestVector(m);

    return 0;
    }
