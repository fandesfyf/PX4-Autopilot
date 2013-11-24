/****************************************************************************
 *
 *   Copyright (c) 2012, 2013 PX4 Development Team. All rights reserved.
 *   Author: Simon Wilks <sjwilks@gmail.com>
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
 * @file frsky_data.c
 * @author Stefan Rado <px4@sradonia.net>
 *
 * FrSky telemetry implementation.
 *
 */

#include "frsky_data.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arch/math.h>
#include <geo/geo.h>

#include <uORB/topics/battery_status.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/vehicle_global_position.h>


static int battery_sub = -1;
static int sensor_sub = -1;
static int global_position_sub = -1;


/**
 * Initializes the uORB subscriptions.
 */
void frsky_init()
{
	battery_sub = orb_subscribe(ORB_ID(battery_status));
	global_position_sub = orb_subscribe(ORB_ID(vehicle_global_position));
	sensor_sub = orb_subscribe(ORB_ID(sensor_combined));
}

/**
 * Sends a 0x5E start/stop byte.
 */
static void
frsky_send_startstop(int uart)
{
	static const uint8_t c = 0x5E;
	write(uart, &c, sizeof(c));
}

/**
 * Sends one byte, performing byte-stuffing if necessary.
 */
static void
frsky_send_byte(int uart, uint8_t value)
{
	const uint8_t x5E[] = {0x5D, 0x3E};
	const uint8_t x5D[] = {0x5D, 0x3D};

	switch (value)
	{
	case 0x5E:
		write(uart, x5E, sizeof(x5E));
		break;

	case 0x5D:
		write(uart, x5D, sizeof(x5D));
		break;

	default:
		write(uart, &value, sizeof(value));
		break;
	}
}

/**
 * Sends one data id/value pair.
 */
static void
frsky_send_data(int uart, uint8_t id, uint16_t data)
{
	frsky_send_startstop(uart);

	frsky_send_byte(uart, id);
	frsky_send_byte(uart, data);      /* Low */
	frsky_send_byte(uart, data >> 8); /* High */
}

/**
 * Sends frame 1 (every 200ms):
 *   acceleration values, altitude (vario), temperatures, current & voltages, RPM
 */
void frsky_send_frame1(int uart)
{
	/* get a local copy of the current sensor values */
	struct sensor_combined_s raw;
	memset(&raw, 0, sizeof(raw));
	orb_copy(ORB_ID(sensor_combined), sensor_sub, &raw);

	/* get a local copy of the battery data */
	struct battery_status_s battery;
	memset(&battery, 0, sizeof(battery));
	orb_copy(ORB_ID(battery_status), battery_sub, &battery);

	/* send formatted frame */
	// TODO
	frsky_send_data(uart, FRSKY_ID_ACCEL_X, raw.accelerometer_m_s2[0] * 100);
	frsky_send_data(uart, FRSKY_ID_ACCEL_Y, raw.accelerometer_m_s2[1] * 100);
	frsky_send_data(uart, FRSKY_ID_ACCEL_Z, raw.accelerometer_m_s2[2] * 100);

	frsky_send_data(uart, FRSKY_ID_BARO_ALT_BP, raw.baro_alt_meter);
	frsky_send_data(uart, FRSKY_ID_BARO_ALT_AP, (raw.baro_alt_meter - (int)raw.baro_alt_meter) * 1000.0f);

	frsky_send_data(uart, FRSKY_ID_TEMP1, raw.baro_temp_celcius);
	frsky_send_data(uart, FRSKY_ID_TEMP2, 0);

	frsky_send_data(uart, FRSKY_ID_VOLTS, 0); /* cell voltage. 4 bits cell number, 12 bits voltage in 0.2V steps, scale 0-4.2V */
	frsky_send_data(uart, FRSKY_ID_CURRENT, battery.current_a);

	frsky_send_data(uart, FRSKY_ID_VOLTS_BP, battery.voltage_v);
	frsky_send_data(uart, FRSKY_ID_VOLTS_AP, (battery.voltage_v - (int)battery.voltage_v) * 1000.0f);

	frsky_send_data(uart, FRSKY_ID_RPM, 0);

	frsky_send_startstop(uart);
}

/**
 * Sends frame 2 (every 1000ms):
 *   course, latitude, longitude, speed, altitude (GPS), fuel level
 */
void frsky_send_frame2(int uart)
{
 	/* get a local copy of the battery data */
	struct vehicle_global_position_s global_pos;
	memset(&global_pos, 0, sizeof(global_pos));
	orb_copy(ORB_ID(vehicle_global_position), global_position_sub, &global_pos);

	/* send formatted frame */
	// TODO
	float course = 0, lat = 0, lon = 0, speed = 0, alt = 0, sec = 0;
	if (global_pos.valid)
	{
		course = (global_pos.yaw + M_PI_F) / M_PI_F * 180.0f;
		// TODO: latitude, longitude
		speed = sqrtf(global_pos.vx * global_pos.vx + global_pos.vy * global_pos.vy);
		alt = global_pos.alt;
	}

	frsky_send_data(uart, FRSKY_ID_GPS_COURS_BP, course);
	frsky_send_data(uart, FRSKY_ID_GPS_COURS_AP, (course - (int)course) * 100.0f);

	frsky_send_data(uart, FRSKY_ID_GPS_LAT_BP, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_LAT_AP, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_LAT_NS, 0);

	frsky_send_data(uart, FRSKY_ID_GPS_LONG_BP, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_LONG_AP, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_LONG_EW, 0);

	frsky_send_data(uart, FRSKY_ID_GPS_SPEED_BP, speed);
	frsky_send_data(uart, FRSKY_ID_GPS_SPEED_AP, (speed - (int)speed) * 100.0f);

	frsky_send_data(uart, FRSKY_ID_GPS_ALT_BP, alt);
	frsky_send_data(uart, FRSKY_ID_GPS_ALT_AP, (alt - (int)alt) * 100.0f);

	frsky_send_data(uart, FRSKY_ID_FUEL, 0);

	frsky_send_data(uart, FRSKY_ID_GPS_SEC, 0);

	frsky_send_startstop(uart);
}

/**
 * Sends frame 3 (every 5000ms):
 *   date, time
 */
void frsky_send_frame3(int uart)
{
 	/* get a local copy of the battery data */
	struct vehicle_global_position_s global_pos;
	memset(&global_pos, 0, sizeof(global_pos));
	orb_copy(ORB_ID(vehicle_global_position), global_position_sub, &global_pos);

	/* send formatted frame */
	// TODO
	frsky_send_data(uart, FRSKY_ID_GPS_DAY_MONTH, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_YEAR, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_HOUR_MIN, 0);
	frsky_send_data(uart, FRSKY_ID_GPS_SEC, 0);

	frsky_send_startstop(uart);
}
