# Understanding MCCI Catena data sent on port 1 format 0x1f

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC -->

- [Understanding MCCI Catena data sent on port 1 format 0x1f](#understanding-mcci-catena-data-sent-on-port-1-format-0x1f)
	- [Overall Message Format](#overall-message-format)
	- [Bitmap fields and associated fields](#bitmap-fields-and-associated-fields)
		- [Battery Voltage (field 0)](#battery-voltage-field-0)
		- [System Voltage (field 1)](#system-voltage-field-1)
		- [Boot counter (field 2)](#boot-counter-field-2)
		- [Temperature (field 3)](#temperature-field-3)
		- [Differential Pressure (field 4)](#differential-pressure-field-4)
	- [Data Formats](#data-formats)
		- [uint16](#uint16)
		- [int16](#int16)
		- [uint8](#uint8)
		- [uflt16](#uflt16)
		- [sflt16](#sflt16)
	- [Test Vectors](#test-vectors)
		- [Test vector generator](#test-vector-generator)
	- [The Things Network Console decoding script](#the-things-network-console-decoding-script)
	- [Node-RED Decoding Script](#node-red-decoding-script)
	- [Meta](#meta)
		- [Support Open Source Hardware and Software](#support-open-source-hardware-and-software)
		- [Trademarks](#trademarks)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Overall Message Format

Port 1 format 0x1F uplink messages are sent by `sdp_lorawan.ino` and related sketches in the [MCCI Catena SDP](https://github.com/mcci-catena/MCCI_Catena_SDP) library. As demos, we use the discriminator byte in the same way as many of the sketches in the Catena-Sketches collection.

Each message has the following layout.

byte | description
:---:|:---
0    | magic number 0x1F
1    | bitmap encoding the fields that follow
2..n | data bytes; use bitmap to map these bytes onto fields.

Each bit in byte 1 represents whether a corresponding field in bytes 2..n is present. If all bits are clear, then no data bytes are present. If bit 0 is set, then field 0 is present; if bit 1 is set, then field 1 is present, and so forth. If a field is omitted, all bytes for that field are omitted.

## Bitmap fields and associated fields

The bitmap byte has the following interpretation. `int16`, `uint16`, etc. are defined after the table.

Bitmap bit | Length of corresponding field (bytes) | Data format |Description
:---:|:---:|:---:|:----
0 | 2 | [int16](#int16) | [Battery voltage](#battery-voltage-field-0)
1 | 2 | [int16](#int16) | [System voltage](#sys-voltage-field-1)
2 | 1 | [uint8](#uint8) | [Boot counter](#boot-counter-field-2)
3 | 2 | [int16](#int16) | [Temperature](#temperature-field-3)
4 | 4 | [int16](#uint16), [uint16](#uint16) | [Differential Pressure](differential-pressure-field-4)
5 | n/a | _reserved_ | Reserved for future use.
6 | n/a | _reserved_ | Reserved for future use.
7 | n/a | _reserved_ | Reserved for future use.

### Battery Voltage (field 0)

Field 0, if present, carries the current battery voltage. To get the voltage, extract the int16 value, and divide by 4096.0. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### System Voltage (field 1)

Field 1, if present, carries the current System voltage. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

_Note:_ this field is not transmitted by some versions of the sketches.

### Boot counter (field 2)

Field 2, if present, is a counter of number of recorded system reboots, modulo 256.

### Temperature (field 3)

Field 3, if present, is the temperature as read from the SDP sensor.

The first two bytes are a [`int16`](#int16) representing the temperature (divide by 200 to get degrees Celsius).

### Differential Pressure (field 4)

Field 4, if present, has the current differential pressure reading as a [`sflt16`](#sflt16).  `sflt16` values respresent values in the interval (-1, 1).  Get the pressure in Pascal by multiplying by 32768/60, or 546.133.

## Data Formats

All multi-byte data is transmitted with the most significant byte first (big-endian format).  Comments on the individual formats follow.

### uint16

an integer from 0 to 65536.

### int16

a signed integer from -32,768 to 32,767, in two's complement form. (Thus 0..0x7FFF represent 0 to 32,767; 0x8000 to 0xFFFF represent -32,768 to -1).

### uint8

an integer from 0 to 255.

### uflt16

A unsigned floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

bits | description
:---:|:---
15..12 | binary exponent `b`
11..0 | fraction `f`

The floating point number is computed by computing `f`/4096 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated.

For example, if the transmitted message contains 0x1A, 0xAB, the equivalent floating point number is found as follows.

1. The full 16-bit number is 0x1AAB.
2. `b`  is therefore 0x1, and `b`-15 is -14.  2^-14 is 1/32768
3. `f` is 0xAAB. 0xAAB/4096 is 0.667
4. `f * 2^(b-15)` is therefore 0.6667/32768 or 0.0000204

Floating point mavens will immediately recognize:

- There is no sign bit; all numbers are positive.
- Numbers do not need to be normalized (although in practice they always are).
- The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048.. 4095, and then transmit only `f - 2048`, saving a bit. However, this complicated the handling of gradual underflow; see next point.)
- Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

### sflt16

A `sflt16` datum represents an unsigned floating point number in the range [0, 1.0), transmitted as a 16-bit field. The encoded field is interpreted as follows:

bits | description
:---:|:---
15 | Sign bit
14..11 | binary exponent `b`
10..0 | fraction `f`

The corresponding floating point value is computed by computing `f`/2048 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated. However, it is similar to IEEE format in that it uses sign-magnitude rather than twos-complement for negative values.

For example, if the data value is 0x8D, 0x55, the equivalent floating point number is found as follows.

1. The full 16-bit number is 0x8D55.
2. Bit 15 is 1, so this is a negative value.
3. `b`  is 1, and `b`-15 is -14.  2^-14 is 1/16384
4. `f` is 0x555. 0x555/2048 = 1365/2048 is 0.667
5. `f * 2^(b-15)` is therefore 0.667/16384 or 0.00004068
6. Since the number is negative, the value is -0.00004068

Floating point mavens will immediately recognize:

* This format uses sign/magnitude representation for negative numbers.
* Numbers do not need to be normalized (although in practice they always are).
* The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048..4095, and then transmit only `f - 2048`, saving a bit. However, this complicates the handling of gradual underflow; see next point.)
* Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

## Test Vectors

The following input data can be used to test decoders.

   `1F 01 18 00`

   ```json
   {
     "vBat": 1.5
   }
   ```

   `1f 02 34 cd`

   ```json
   {
     "vSys": 3.300048828125
   }
   ```

   `1f 04 2a`

   ```json
   {
     "boot": 42
   }
   ```

   `1f 08 10 7c`

   ```json
   {
     "TemperatureC": 21.1
   }```

   `1f 10 6f 53`

   ```json
   {
     "DifferentialPressure": 125
   }
   ```

   `1f 1f 13 96 34 cd 31 14 8c 6e 07`

   ```json
   {
     "Boot": 49,
     "DifferentialPressure": 102.86666666666666,
     "TemperatureC": 26.3,
     "Vbattery": 1.22412109375,
     "Vsystem": 3.300048828125
   }
   ```

### Test vector generator

This repository contains a simple C++ file for generating test vectors.

Build it from the command line. Using Visual C++:

```console
C> cl /EHsc message-port1-format-1f-test.cpp
Microsoft (R) C/C++ Optimizing Compiler Version 19.26.28806 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

message-port1-format-1f-test.cpp
Microsoft (R) Incremental Linker Version 14.26.28806.0
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:message-port1-format-1f-test.exe
message-port1-format-1f-test.obj
```

Using GCC or Clang on Linux:

```bash
make message-port1-format-20-test
```

(The default make rules should work.)

For usage, read the source or the check the input vector generation file `message-port1-format-1f.vec`.

To run it against the test vectors, try:

```console
$ message-port1-format-1f-test < message-port1-format-1f.vec
Input a line with name/values pairs
Vbat 1.5 .
1f 01 18 00
Vsys 3.3 .
1f 02 34 cd
Boot 42 .
1f 04 2a
T 21.1 .
1f 08 10 7c
deltaP 125 .
1f 10 6f 53
Vbat 1.2241 Vsys 3.3 Boot 49 T 26.3 deltaP 102.866 .
1f 1f 13 96 34 cd 31 14 8c 6e 07
```

## The Things Network Console decoding script

The repository contains a generic script that decodes messages in this format, for [The Things Network console](https://console.thethingsnetwork.org).

You can get the latest version on GitHub:

- in [raw form](https://raw.githubusercontent.com/mcci-catena/MCCI_Catena_SDP/master/extra/message-port1-format-1f-decoder-ttn.js)
- or [view it](https://github.com/mcci-catena/MCCI_Catena_SDP/blob/master/extra/message-port1-format-1f-decoder-ttn.js)

## Node-RED Decoding Script

A Node-RED script to decode this data is part of this repository. You can download the latest version from GitHub:

- in [raw form](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-PMS7003/master/extra/catena-message-port1-20-decoder-node-red.js)
- or [view it](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-PMS7003/blob/master/extra/catena-message-port1-20-decoder-node-red.js)

## Meta

### Support Open Source Hardware and Software

MCCI invests time and resources providing this open source code, please support MCCI and open-source hardware by purchasing products from MCCI, Adafruit and other open-source hardware/software vendors!

For information about MCCI's products, please visit [store.mcci.com](https://store.mcci.com/).

### Trademarks

MCCI and MCCI Catena are registered trademarks of MCCI Corporation. All other marks are the property of their respective owners.
