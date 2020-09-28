# SDP810-125Pa Simple Example Sketch

The sketch in this directory (found [here](https://github.com/mcci-catena/MCCI_Catena_SDP/examples/sdp_simple) on GitHub) is a very simple demo of how to capture data from a Sensirion SDP810 sensor and print it on the serial port. It requires no additional libraries.

## Setting up the sketch

The sketch sets the serial port to 115k baud.

It uses the newer Arduino BSP convention of polling `Serial` to wait for a USB serial port to be connected. Your BSP might not support it; if so, comment it out.

```c+++
// wait for USB to be attached.
while (! Serial)
    yield();
```

If you're using an SDP800-family device, you're all set. If you are using an SDP31 or SDP32, you must change the address used in the sketch.

Find this line:

```c++
cSDP gSdp {Wire, cSDP::Address::SDP8xx};
```

Change it to one of the following, depending on how you've configured your device.

- If you've grounded the `ADDR` pin of the device, use:

   ```c++
   cSDP gSdp {Wire, cSDP::Address::SDP3x_A};
   ```

- If you're using a 1.2k Ohm pulldown on `ADDR`, use:

   ```c++
   cSDP gSdp {Wire, cSDP::Address::SDP3x_B};
   ```

- If you're using a 2.7k Ohm pulldown on `ADDR`, use:

   ```c++
   cSDP gSdp {Wire, cSDP::Address::SDP3x_C};
   ```

## What to expect

The sketch reads and displays the temperature and differential pressure once a second. You should see something like this.

```log
SDP Simple Test
Found sensor SDP810-125Pa, serial number 2020128187
T(F)=76.77  differential pressure=0.03 Pa
T(F)=76.78  differential pressure=-0.10 Pa
T(F)=76.78  differential pressure=-0.28 Pa
T(F)=76.84  differential pressure=-0.19 Pa
T(F)=76.83  differential pressure=79.34 Pa
T(F)=76.79  differential pressure=10.86 Pa
T(F)=76.75  differential pressure=0.01 Pa
T(F)=76.80  differential pressure=-0.17 Pa
T(F)=76.74  differential pressure=-8.33 Pa
T(F)=76.75  differential pressure=-14.94 Pa
T(F)=76.80  differential pressure=-0.49 Pa
T(F)=76.78  differential pressure=-0.12 Pa
```

Our setup has 10" of 4mm i.d. tubing attached to the positive side of the SDP810-125Pa.

The positive pulse in pressure was caused by gently blowing into the positive tube.

The negative pulse was caused by blowing across the positive tube (as if playing a flute). (You can, of course, also get a negative pulse by attaching a tube to the negative side.)

If measuring continuously with this program, you will see self-heating in the sensor (i.e., the temperature will be higher than ambient). If you use a more advanced sketch that puts the sensor to sleep between measurements, or even powers it down, you'll see less rise.

## Meta

### Support Open Source Hardware and Software

MCCI invests time and resources providing this open source code, please support MCCI and open-source hardware by purchasing products from MCCI, Adafruit and other open-source hardware/software vendors!

For information about MCCI's products, please visit [store.mcci.com](https://store.mcci.com/).

### Trademarks

MCCI and MCCI Catena are registered trademarks of MCCI Corporation. All other marks are the property of their respective owners.
