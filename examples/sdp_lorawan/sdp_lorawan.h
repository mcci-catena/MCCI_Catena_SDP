/*

Module:	sdp_lorawan.h

Function:
	Global definitions for sdp_lorawan.

Copyright and License:
	This file copyright (C) 2020 by

		MCCI Corporation
		3520 Krums Corners Road
		Ithaca, NY  14850

	See accompanying LICENSE file for copyright and license information.

Author:
	Terry Moore, MCCI Corporation	September 2020

*/

#ifndef _sdp_lorawan_h_
#define _sdp_lorawan_h_	/* prevent multiple includes */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Catena.h>
#include <Catena_Led.h>
#include <Catena_Log.h>

extern McciCatena::Catena gCatena;
extern McciCatena::Catena::LoRaWAN gLoRaWAN;
extern McciCatena::StatusLed gLed;
extern SPIClass gSPI2;
extern bool gfFlash;
// extern McciCatena::cLog gLog;

#endif /* _sdp_lorawan_h_ */
