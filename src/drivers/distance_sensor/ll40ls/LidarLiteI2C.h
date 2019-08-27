/****************************************************************************
 *
 *   Copyright (c) 2014-2019 PX4 Development Team. All rights reserved.
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
 * @file LidarLiteI2C.h
 * @author Allyson Kreft
 *
 * Driver for the PulsedLight Lidar-Lite range finders connected via I2C.
 */

#pragma once

#include "LidarLite.h"

#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <drivers/device/i2c.h>

/* Configuration Constants */
static constexpr uint8_t LL40LS_BASEADDR	= 0x62;	/* 7-bit address */
static constexpr uint8_t LL40LS_BASEADDR_OLD	= 0x42;	/* previous 7-bit address */

/* LL40LS Registers addresses */
static constexpr uint8_t LL40LS_MEASURE_REG	= 0x00;	/* Measure range register */
static constexpr uint8_t LL40LS_MSRREG_RESET	= 0x00;	/* reset to power on defaults */
static constexpr uint8_t LL40LS_MSRREG_ACQUIRE	=
	0x04;	/* Value to initiate a measurement, varies based on sensor revision */
static constexpr uint8_t LL40LS_DISTHIGH_REG	= 0x0F;	/* High byte of distance register, auto increment */
static constexpr uint8_t LL40LS_AUTO_INCREMENT	= 0x80;
static constexpr uint8_t LL40LS_HW_VERSION	= 0x41;
static constexpr uint8_t LL40LS_SW_VERSION	= 0x4f;
static constexpr uint8_t LL40LS_SIGNAL_STRENGTH_REG	= 0x0e;
static constexpr uint8_t LL40LS_PEAK_STRENGTH_REG	= 0x0c;
static constexpr uint8_t LL40LS_UNIT_ID_HIGH	= 0x16;
static constexpr uint8_t LL40LS_UNIT_ID_LOW	= 0x17;

static constexpr uint8_t LL40LS_SIG_COUNT_VAL_REG	= 0x02;	/* Maximum acquisition count register */
static constexpr uint8_t LL40LS_SIG_COUNT_VAL_MAX	= 0xFF;	/* Maximum acquisition count max value */

static constexpr int LL40LS_SIGNAL_STRENGTH_LOW =
	24;	// Minimum (relative) signal strength value for accepting a measurement
static constexpr int LL40LS_PEAK_STRENGTH_LOW	= 135;	// Minimum peak strength raw value for accepting a measurement
static constexpr int LL40LS_PEAK_STRENGTH_HIGH	= 234;	// Max peak strength raw value


class LidarLiteI2C : public LidarLite, public device::I2C, public px4::ScheduledWorkItem
{
public:
	LidarLiteI2C(int bus, uint8_t rotation = distance_sensor_s::ROTATION_DOWNWARD_FACING, int address = LL40LS_BASEADDR);
	virtual ~LidarLiteI2C();

	int		init() override;

	void		print_registers() override;

protected:
	int		probe() override;
	int		read_reg(uint8_t reg, uint8_t &val);
	int		write_reg(uint8_t reg, uint8_t val);

	int		measure() override;
	int		reset_sensor() override;

private:

	/**
	 * LidarLite specific transfer function. This is needed
	 * to avoid a stop transition with SCL high
	 *
	 * @param send		Pointer to bytes to send.
	 * @param send_len	Number of bytes to send.
	 * @param recv		Pointer to buffer for bytes received.
	 * @param recv_len	Number of bytes to receive.
	 * @return		OK if the transfer was successful, -errno
	 *			otherwise.
	 */
	int                 lidar_transfer(const uint8_t *send, unsigned send_len, uint8_t *recv, unsigned recv_len);

	/**
	* Test whether the device supported by the driver is present at a
	* specific address.
	*
	* @param address    The I2C bus address to probe.
	* @return       True if the device is present.
	*/
	int                 probe_address(uint8_t address);

	/**
	* Initialise the automatic measurement state machine and start it.
	*
	* @note This function is called at open and error time.  It might make sense
	*       to make it more aggressive about resetting the bus in case of errors.
	*/
	void                start() override;

	/**
	* Stop the automatic measurement state machine.
	*/
	void                stop() override;

	/**
	* Perform a poll cycle; collect from the previous measurement
	* and start a new one.
	*/
	void                Run() override;
	int                 collect() override;


	bool		_sensor_ok{false};
	bool		_collect_phase{false};

	uint16_t	_zero_counter{0};
	uint64_t	_acquire_time_usec{0};

	bool		_pause_measurements{false};

	uint8_t		_hw_version{0};
	uint8_t		_sw_version{0};
	uint16_t	_unit_id{0};

};
