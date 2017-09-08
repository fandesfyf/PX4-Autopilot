/****************************************************************************
 *
 *   Copyright (c) 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * RPI internal definitions
 */

#pragma once

#define BOARD_OVERRIDE_UUID "RPIID       " // must be of length 12 (PX4_CPU_UUID_BYTE_LENGTH)
#define BOARD_OVERRIDE_MFGUID BOARD_OVERRIDE_UUID
#define BOARD_OVERRIDE_PX4_GUID "00000000" BOARD_OVERRIDE_UUID // must be of length 18 (PX4_GUID_BYTE_LENGTH)

#define BOARD_NAME "RPI"

#define ADC_BATTERY_VOLTAGE_CHANNEL 2
#define ADC_BATTERY_CURRENT_CHANNEL 3

#define BOARD_BATTERY1_V_DIV   (10.177939394f)
#define BOARD_BATTERY1_A_PER_V (15.391030303f)

#define BOARD_HAS_NO_RESET
#define BOARD_HAS_NO_BOOTLOADER

#define BOARD_MAX_LEDS 1 // Number external of LED's this board has

/*
 * I2C busses
 */
#define PX4_I2C_BUS_EXPANSION	1
#define PX4_NUMBER_I2C_BUSES 1

#include <system_config.h>
#include "../common/board_common.h"
