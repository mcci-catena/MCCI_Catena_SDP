# SDP-810-125Pa LoRaWAN&reg; Example Sketch

The sketch in this directory (found [here](https://github.com/mcci-catena/MCCI_Catena_SDP/examples/sdp_lorawan) on GitHub) is a fully-worked example of how to capture data from an SDP810 sensor and transmit it via LoRaWAN. It requires the [MCCI_Catena_Arduino_Platform](https://github.mcci.com/mcci-catena/Catena-Arduino-Platform) library.

<!-- TOC depthFrom:2 updateOnSave:true -->

- [Serial Port Commands](#serial-port-commands)
	- [`debugflags`](#debugflags)
	- [`run`](#run)
	- [`stop`](#stop)
	- [`system configure operatingflags`](#system-configure-operatingflags)
- [Operating flags](#operating-flags)
	- [Sleep and the serial port](#sleep-and-the-serial-port)
- [Data Format](#data-format)
- [](#)

<!-- /TOC -->

## Serial Port Commands

The sketch responds to commands from the serial port. In addition to the commands that are part of the Catena-Arduino-Platform, the sketch also has the following commands.

### `debugflags`

This command prints or changes the debug flag mask. If entered without arguments, it displays the current flags. If entered with arguments, it changes the debug flags. The flags are a 32-bit word, and may be entered in decimal, hexadecimal (with a `0x` prefix), or octal (with a `0` prefix).  The defined bits are:

Bit |  Mask | Name | Meaning
:--:|:-----:|------|---------
  0 |  `0x01` | `kBug` | Error messages displayed for bugs
  1 |  `0x02` | `kError` | Error messages displayed for runtime errors
  2 |  `0x04` | `kWarning` | Runtime warnings
  3 |  `0x08` | `kTrace` | Messages tracing changes of state, e.g. FSM transitions.
  4 |  `0x10` | `kInfo` | Informational messages.
5..31 | `0xFFFFFFE0` | N/A | Not used.

### `run`

This command starts the measure/transmit loop. It is automatically started on bootup if the LoRaWAN system is configured. Otherwise, the measure/transmit loop will remain in an idle state.

### `stop`

This command stops the measure/transmit loop.

### `system configure operatingflags`

This command is used to set the system operating flags in FRAM. This application only uses bit 0. If bit zero is set, it enables "stand-alone mode". In this mode, the device uses deep sleeps in between transmissions. While sleeping, the serial port is disabled.

If bit zero is clear, the device is in "development mode". In this mode, the device stays partially awake between transmissions, and will respond to the serial port. This mode takes more power, but is more convenient during development.

## Data Format

The device transmits data on port 1, and uses the first byte as a format discriminator. The byte is `0x1F`.  See 

## 