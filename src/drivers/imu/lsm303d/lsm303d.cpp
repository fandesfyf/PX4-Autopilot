/****************************************************************************
 *
 *   Copyright (c) 2013-2019 PX4 Development Team. All rights reserved.
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
 * @file lsm303d.cpp
 * Driver for the ST LSM303D MEMS accelerometer / magnetometer connected via SPI.
 */

#include <drivers/device/spi.h>
#include <ecl/geo/geo.h>
#include <perf/perf_counter.h>
#include <px4_getopt.h>
#include <px4_work_queue/ScheduledWorkItem.hpp>
#include <lib/drivers/accelerometer/PX4Accelerometer.hpp>
#include <lib/drivers/magnetometer/PX4Magnetometer.hpp>

/* SPI protocol address bits */
#define DIR_READ			(1<<7)
#define DIR_WRITE			(0<<7)
#define ADDR_INCREMENT			(1<<6)

/* register addresses: A: accel, M: mag, T: temp */
#define ADDR_WHO_AM_I			0x0F
#define WHO_I_AM			0x49

#define ADDR_OUT_TEMP_L			0x05
#define ADDR_OUT_TEMP_H			0x06
#define ADDR_STATUS_M			0x07
#define ADDR_OUT_X_L_M          	0x08
#define ADDR_OUT_X_H_M          	0x09
#define ADDR_OUT_Y_L_M          	0x0A
#define ADDR_OUT_Y_H_M			0x0B
#define ADDR_OUT_Z_L_M			0x0C
#define ADDR_OUT_Z_H_M			0x0D

#define ADDR_INT_CTRL_M			0x12
#define ADDR_INT_SRC_M			0x13
#define ADDR_REFERENCE_X		0x1c
#define ADDR_REFERENCE_Y		0x1d
#define ADDR_REFERENCE_Z		0x1e

#define ADDR_STATUS_A			0x27
#define ADDR_OUT_X_L_A			0x28
#define ADDR_OUT_X_H_A			0x29
#define ADDR_OUT_Y_L_A			0x2A
#define ADDR_OUT_Y_H_A			0x2B
#define ADDR_OUT_Z_L_A			0x2C
#define ADDR_OUT_Z_H_A			0x2D

#define ADDR_CTRL_REG0			0x1F
#define ADDR_CTRL_REG1			0x20
#define ADDR_CTRL_REG2			0x21
#define ADDR_CTRL_REG3			0x22
#define ADDR_CTRL_REG4			0x23
#define ADDR_CTRL_REG5			0x24
#define ADDR_CTRL_REG6			0x25
#define ADDR_CTRL_REG7			0x26

#define ADDR_FIFO_CTRL			0x2e
#define ADDR_FIFO_SRC			0x2f

#define ADDR_IG_CFG1			0x30
#define ADDR_IG_SRC1			0x31
#define ADDR_IG_THS1			0x32
#define ADDR_IG_DUR1			0x33
#define ADDR_IG_CFG2			0x34
#define ADDR_IG_SRC2			0x35
#define ADDR_IG_THS2			0x36
#define ADDR_IG_DUR2			0x37
#define ADDR_CLICK_CFG			0x38
#define ADDR_CLICK_SRC			0x39
#define ADDR_CLICK_THS			0x3a
#define ADDR_TIME_LIMIT			0x3b
#define ADDR_TIME_LATENCY		0x3c
#define ADDR_TIME_WINDOW		0x3d
#define ADDR_ACT_THS			0x3e
#define ADDR_ACT_DUR			0x3f

#define REG1_RATE_BITS_A		((1<<7) | (1<<6) | (1<<5) | (1<<4))
#define REG1_POWERDOWN_A		((0<<7) | (0<<6) | (0<<5) | (0<<4))
#define REG1_RATE_3_125HZ_A		((0<<7) | (0<<6) | (0<<5) | (1<<4))
#define REG1_RATE_6_25HZ_A		((0<<7) | (0<<6) | (1<<5) | (0<<4))
#define REG1_RATE_12_5HZ_A		((0<<7) | (0<<6) | (1<<5) | (1<<4))
#define REG1_RATE_25HZ_A		((0<<7) | (1<<6) | (0<<5) | (0<<4))
#define REG1_RATE_50HZ_A		((0<<7) | (1<<6) | (0<<5) | (1<<4))
#define REG1_RATE_100HZ_A		((0<<7) | (1<<6) | (1<<5) | (0<<4))
#define REG1_RATE_200HZ_A		((0<<7) | (1<<6) | (1<<5) | (1<<4))
#define REG1_RATE_400HZ_A		((1<<7) | (0<<6) | (0<<5) | (0<<4))
#define REG1_RATE_800HZ_A		((1<<7) | (0<<6) | (0<<5) | (1<<4))
#define REG1_RATE_1600HZ_A		((1<<7) | (0<<6) | (1<<5) | (0<<4))

#define REG1_BDU_UPDATE			(1<<3)
#define REG1_Z_ENABLE_A			(1<<2)
#define REG1_Y_ENABLE_A			(1<<1)
#define REG1_X_ENABLE_A			(1<<0)

#define REG2_ANTIALIAS_FILTER_BW_BITS_A	((1<<7) | (1<<6))
#define REG2_AA_FILTER_BW_773HZ_A		((0<<7) | (0<<6))
#define REG2_AA_FILTER_BW_194HZ_A		((0<<7) | (1<<6))
#define REG2_AA_FILTER_BW_362HZ_A		((1<<7) | (0<<6))
#define REG2_AA_FILTER_BW_50HZ_A		((1<<7) | (1<<6))

#define REG2_FULL_SCALE_BITS_A	((1<<5) | (1<<4) | (1<<3))
#define REG2_FULL_SCALE_2G_A	((0<<5) | (0<<4) | (0<<3))
#define REG2_FULL_SCALE_4G_A	((0<<5) | (0<<4) | (1<<3))
#define REG2_FULL_SCALE_6G_A	((0<<5) | (1<<4) | (0<<3))
#define REG2_FULL_SCALE_8G_A	((0<<5) | (1<<4) | (1<<3))
#define REG2_FULL_SCALE_16G_A	((1<<5) | (0<<4) | (0<<3))

#define REG5_ENABLE_T			(1<<7)

#define REG5_RES_HIGH_M			((1<<6) | (1<<5))
#define REG5_RES_LOW_M			((0<<6) | (0<<5))

#define REG5_RATE_BITS_M		((1<<4) | (1<<3) | (1<<2))
#define REG5_RATE_3_125HZ_M		((0<<4) | (0<<3) | (0<<2))
#define REG5_RATE_6_25HZ_M		((0<<4) | (0<<3) | (1<<2))
#define REG5_RATE_12_5HZ_M		((0<<4) | (1<<3) | (0<<2))
#define REG5_RATE_25HZ_M		((0<<4) | (1<<3) | (1<<2))
#define REG5_RATE_50HZ_M		((1<<4) | (0<<3) | (0<<2))
#define REG5_RATE_100HZ_M		((1<<4) | (0<<3) | (1<<2))
#define REG5_RATE_DO_NOT_USE_M	((1<<4) | (1<<3) | (0<<2))

#define REG6_FULL_SCALE_BITS_M	((1<<6) | (1<<5))
#define REG6_FULL_SCALE_2GA_M	((0<<6) | (0<<5))
#define REG6_FULL_SCALE_4GA_M	((0<<6) | (1<<5))
#define REG6_FULL_SCALE_8GA_M	((1<<6) | (0<<5))
#define REG6_FULL_SCALE_12GA_M	((1<<6) | (1<<5))

#define REG7_CONT_MODE_M		((0<<1) | (0<<0))

#define REG_STATUS_A_NEW_ZYXADA		0x08

#define INT_CTRL_M              0x12
#define INT_SRC_M               0x13

/* default values for this device */
#define LSM303D_ACCEL_DEFAULT_RANGE_G			16
#define LSM303D_ACCEL_DEFAULT_RATE			800
#define LSM303D_ACCEL_DEFAULT_ONCHIP_FILTER_FREQ	50
#define LSM303D_ACCEL_DEFAULT_DRIVER_FILTER_FREQ	30

#define LSM303D_MAG_DEFAULT_RANGE_GA			2
#define LSM303D_MAG_DEFAULT_RATE			100

/*
  we set the timer interrupt to run a bit faster than the desired
  sample rate and then throw away duplicates using the data ready bit.
  This time reduction is enough to cope with worst case timing jitter
  due to other timers
 */
#define LSM303D_TIMER_REDUCTION				200

extern "C" { __EXPORT int lsm303d_main(int argc, char *argv[]); }

class LSM303D : public device::SPI, public px4::ScheduledWorkItem
{
public:
	LSM303D(int bus, uint32_t device, enum Rotation rotation);
	virtual ~LSM303D();

	virtual int		init();

	/**
	 * Diagnostics - print some basic information about the driver.
	 */
	void			print_info();

protected:
	virtual int		probe();

private:

	void			Run() override;

	void			start();
	void			stop();
	void			reset();

	/**
	 * disable I2C on the chip
	 */
	void			disable_i2c();

	/**
	 * check key registers for correct values
	 */
	void			check_registers(void);

	/**
	 * Fetch accel measurements from the sensor and update the report ring.
	 */
	void			measureAccelerometer();

	/**
	 * Fetch mag measurements from the sensor and update the report ring.
	 */
	void			measureMagnetometer();

	/**
	 * Read a register from the LSM303D
	 *
	 * @param		The register to read.
	 * @return		The value that was read.
	 */
	uint8_t			read_reg(unsigned reg);

	/**
	 * Write a register in the LSM303D
	 *
	 * @param reg		The register to write.
	 * @param value		The new value to write.
	 */
	void			write_reg(unsigned reg, uint8_t value);

	/**
	 * Modify a register in the LSM303D
	 *
	 * Bits are cleared before bits are set.
	 *
	 * @param reg		The register to modify.
	 * @param clearbits	Bits in the register to clear.
	 * @param setbits	Bits in the register to set.
	 */
	void			modify_reg(unsigned reg, uint8_t clearbits, uint8_t setbits);

	/**
	 * Write a register in the LSM303D, updating _checked_values
	 *
	 * @param reg		The register to write.
	 * @param value		The new value to write.
	 */
	void			write_checked_reg(unsigned reg, uint8_t value);

	/**
	 * Set the LSM303D accel measurement range.
	 *
	 * @param max_g	The measurement range of the accel is in g (9.81m/s^2)
	 *			Zero selects the maximum supported range.
	 * @return		OK if the value can be supported, -ERANGE otherwise.
	 */
	int			accel_set_range(unsigned max_g);

	/**
	 * Set the LSM303D mag measurement range.
	 *
	 * @param max_ga	The measurement range of the mag is in Ga
	 *			Zero selects the maximum supported range.
	 * @return		OK if the value can be supported, -ERANGE otherwise.
	 */
	int			mag_set_range(unsigned max_g);

	/**
	 * Set the LSM303D on-chip anti-alias filter bandwith.
	 *
	 * @param bandwidth The anti-alias filter bandwidth in Hz
	 * 			Zero selects the highest bandwidth
	 * @return		OK if the value can be supported, -ERANGE otherwise.
	 */
	int			accel_set_onchip_lowpass_filter_bandwidth(unsigned bandwidth);

	/**
	 * Set the LSM303D internal accel sampling frequency.
	 *
	 * @param frequency	The internal accel sampling frequency is set to not less than
	 *			this value.
	 *			Zero selects the maximum rate supported.
	 * @return		OK if the value can be supported.
	 */
	int			accel_set_samplerate(unsigned frequency);

	/**
	 * Set the LSM303D internal mag sampling frequency.
	 *
	 * @param frequency	The internal mag sampling frequency is set to not less than
	 *			this value.
	 *			Zero selects the maximum rate supported.
	 * @return		OK if the value can be supported.
	 */
	int			mag_set_samplerate(unsigned frequency);


	PX4Accelerometer	_px4_accel;
	PX4Magnetometer		_px4_mag;

	unsigned		_call_accel_interval{1000000 / LSM303D_ACCEL_DEFAULT_RATE};
	unsigned		_call_mag_interval{1000000 / LSM303D_MAG_DEFAULT_RATE};

	perf_counter_t		_accel_sample_perf;
	perf_counter_t		_accel_sample_interval_perf;
	perf_counter_t		_mag_sample_perf;
	perf_counter_t		_mag_sample_interval_perf;
	perf_counter_t		_bad_registers;
	perf_counter_t		_bad_values;
	perf_counter_t		_accel_duplicates;

	uint8_t			_register_wait{0};

	int16_t			_last_accel[3] {};
	uint8_t			_constant_accel_count{0};

	hrt_abstime		_mag_last_measure{0};

	float			_last_temperature{0.0f};

	// this is used to support runtime checking of key
	// configuration registers to detect SPI bus errors and sensor
	// reset
	static constexpr int LSM303D_NUM_CHECKED_REGISTERS{8};
	uint8_t			_checked_values[LSM303D_NUM_CHECKED_REGISTERS] {};
	uint8_t			_checked_next{0};

};

/*
  list of registers that will be checked in check_registers(). Note
  that ADDR_WHO_AM_I must be first in the list.
 */
static constexpr uint8_t _checked_registers[] = {
	ADDR_WHO_AM_I,
	ADDR_CTRL_REG1,
	ADDR_CTRL_REG2,
	ADDR_CTRL_REG3,
	ADDR_CTRL_REG4,
	ADDR_CTRL_REG5,
	ADDR_CTRL_REG6,
	ADDR_CTRL_REG7
};

LSM303D::LSM303D(int bus, uint32_t device, enum Rotation rotation) :
	SPI("LSM303D", nullptr, bus, device, SPIDEV_MODE3, 11 * 1000 * 1000),
	ScheduledWorkItem(px4::device_bus_to_wq(get_device_id())),
	_px4_accel(get_device_id(), ORB_PRIO_DEFAULT, rotation),
	_px4_mag(get_device_id(), ORB_PRIO_LOW, rotation),
	_accel_sample_perf(perf_alloc(PC_ELAPSED, "lsm303d: acc_read")),
	_accel_sample_interval_perf(perf_alloc(PC_INTERVAL, "lsm303d: acc interval")),
	_mag_sample_perf(perf_alloc(PC_ELAPSED, "lsm303d: mag_read")),
	_mag_sample_interval_perf(perf_alloc(PC_INTERVAL, "lsm303d: mag interval")),
	_bad_registers(perf_alloc(PC_COUNT, "lsm303d: bad_reg")),
	_bad_values(perf_alloc(PC_COUNT, "lsm303d: bad_val")),
	_accel_duplicates(perf_alloc(PC_COUNT, "lsm303d: acc_dupe"))
{
	_px4_accel.set_device_type(DRV_ACC_DEVTYPE_LSM303D);
	_px4_mag.set_device_type(DRV_MAG_DEVTYPE_LSM303D);
}

LSM303D::~LSM303D()
{
	// make sure we are truly inactive
	stop();

	// delete the perf counter
	perf_free(_accel_sample_perf);
	perf_free(_accel_sample_interval_perf);
	perf_free(_mag_sample_perf);
	perf_free(_mag_sample_interval_perf);
	perf_free(_bad_registers);
	perf_free(_bad_values);
	perf_free(_accel_duplicates);
}

int
LSM303D::init()
{
	/* do SPI init (and probe) first */
	int ret = SPI::init();

	if (ret != OK) {
		PX4_ERR("SPI init failed (%i)", ret);
		return PX4_ERROR;
	}

	reset();

	start();

	return ret;
}

void
LSM303D::disable_i2c(void)
{
	uint8_t a = read_reg(0x02);
	write_reg(0x02, (0x10 | a));
	a = read_reg(0x02);
	write_reg(0x02, (0xF7 & a));
	a = read_reg(0x15);
	write_reg(0x15, (0x80 | a));
	a = read_reg(0x02);
	write_reg(0x02, (0xE7 & a));
}

void
LSM303D::reset()
{
	// ensure the chip doesn't interpret any other bus traffic as I2C
	disable_i2c();

	// enable accel
	write_checked_reg(ADDR_CTRL_REG1, REG1_X_ENABLE_A | REG1_Y_ENABLE_A | REG1_Z_ENABLE_A | REG1_BDU_UPDATE |
			  REG1_RATE_800HZ_A);

	// enable mag
	write_checked_reg(ADDR_CTRL_REG7, REG7_CONT_MODE_M);
	write_checked_reg(ADDR_CTRL_REG5, REG5_RES_HIGH_M | REG5_ENABLE_T);
	write_checked_reg(ADDR_CTRL_REG3, 0x04); // DRDY on ACCEL on INT1
	write_checked_reg(ADDR_CTRL_REG4, 0x04); // DRDY on MAG on INT2

	accel_set_range(LSM303D_ACCEL_DEFAULT_RANGE_G);
	accel_set_samplerate(LSM303D_ACCEL_DEFAULT_RATE);

	// we setup the anti-alias on-chip filter as 50Hz. We believe
	// this operates in the analog domain, and is critical for
	// anti-aliasing. The 2 pole software filter is designed to
	// operate in conjunction with this on-chip filter
	accel_set_onchip_lowpass_filter_bandwidth(LSM303D_ACCEL_DEFAULT_ONCHIP_FILTER_FREQ);

	mag_set_range(LSM303D_MAG_DEFAULT_RANGE_GA);
	mag_set_samplerate(LSM303D_MAG_DEFAULT_RATE);
}

int
LSM303D::probe()
{
	// read dummy value to void to clear SPI statemachine on sensor
	read_reg(ADDR_WHO_AM_I);

	// verify that the device is attached and functioning
	if (read_reg(ADDR_WHO_AM_I) == WHO_I_AM) {
		_checked_values[0] = WHO_I_AM;
		return OK;
	}

	return -EIO;
}

uint8_t
LSM303D::read_reg(unsigned reg)
{
	uint8_t cmd[2] {};
	cmd[0] = reg | DIR_READ;

	transfer(cmd, cmd, sizeof(cmd));

	return cmd[1];
}

void
LSM303D::write_reg(unsigned reg, uint8_t value)
{
	uint8_t	cmd[2] {};

	cmd[0] = reg | DIR_WRITE;
	cmd[1] = value;

	transfer(cmd, nullptr, sizeof(cmd));
}

void
LSM303D::write_checked_reg(unsigned reg, uint8_t value)
{
	write_reg(reg, value);

	for (uint8_t i = 0; i < LSM303D_NUM_CHECKED_REGISTERS; i++) {
		if (reg == _checked_registers[i]) {
			_checked_values[i] = value;
		}
	}
}

void
LSM303D::modify_reg(unsigned reg, uint8_t clearbits, uint8_t setbits)
{
	uint8_t	val = read_reg(reg);
	val &= ~clearbits;
	val |= setbits;
	write_checked_reg(reg, val);
}

int
LSM303D::accel_set_range(unsigned max_g)
{
	uint8_t setbits = 0;
	uint8_t clearbits = REG2_FULL_SCALE_BITS_A;
	float new_scale_g_digit = 0.0f;

	if (max_g == 0) {
		max_g = 16;
	}

	if (max_g <= 2) {
		// accel_range_m_s2 = 2.0f * CONSTANTS_ONE_G;
		setbits |= REG2_FULL_SCALE_2G_A;
		new_scale_g_digit = 0.061e-3f;

	} else if (max_g <= 4) {
		// accel_range_m_s2 = 4.0f * CONSTANTS_ONE_G;
		setbits |= REG2_FULL_SCALE_4G_A;
		new_scale_g_digit = 0.122e-3f;

	} else if (max_g <= 6) {
		// accel_range_m_s2 = 6.0f * CONSTANTS_ONE_G;
		setbits |= REG2_FULL_SCALE_6G_A;
		new_scale_g_digit = 0.183e-3f;

	} else if (max_g <= 8) {
		// accel_range_m_s2 = 8.0f * CONSTANTS_ONE_G;
		setbits |= REG2_FULL_SCALE_8G_A;
		new_scale_g_digit = 0.244e-3f;

	} else if (max_g <= 16) {
		// accel_range_m_s2 = 16.0f * CONSTANTS_ONE_G;
		setbits |= REG2_FULL_SCALE_16G_A;
		new_scale_g_digit = 0.732e-3f;

	} else {
		return -EINVAL;
	}

	float accel_range_scale = new_scale_g_digit * CONSTANTS_ONE_G;

	_px4_accel.set_scale(accel_range_scale);

	modify_reg(ADDR_CTRL_REG2, clearbits, setbits);

	return OK;
}

int
LSM303D::mag_set_range(unsigned max_ga)
{
	uint8_t setbits = 0;
	uint8_t clearbits = REG6_FULL_SCALE_BITS_M;
	float new_scale_ga_digit = 0.0f;

	if (max_ga == 0) {
		max_ga = 12;
	}

	if (max_ga <= 2) {
		// mag_range_ga = 2;
		setbits |= REG6_FULL_SCALE_2GA_M;
		new_scale_ga_digit = 0.080e-3f;

	} else if (max_ga <= 4) {
		// mag_range_ga = 4;
		setbits |= REG6_FULL_SCALE_4GA_M;
		new_scale_ga_digit = 0.160e-3f;

	} else if (max_ga <= 8) {
		// mag_range_ga = 8;
		setbits |= REG6_FULL_SCALE_8GA_M;
		new_scale_ga_digit = 0.320e-3f;

	} else if (max_ga <= 12) {
		// mag_range_ga = 12;
		setbits |= REG6_FULL_SCALE_12GA_M;
		new_scale_ga_digit = 0.479e-3f;

	} else {
		return -EINVAL;
	}

	_px4_mag.set_scale(new_scale_ga_digit);

	modify_reg(ADDR_CTRL_REG6, clearbits, setbits);

	return OK;
}

int
LSM303D::accel_set_onchip_lowpass_filter_bandwidth(unsigned bandwidth)
{
	uint8_t setbits = 0;
	uint8_t clearbits = REG2_ANTIALIAS_FILTER_BW_BITS_A;

	if (bandwidth == 0) {
		bandwidth = 773;
	}

	if (bandwidth <= 50) {
		// accel_onchip_filter_bandwith = 50;
		setbits |= REG2_AA_FILTER_BW_50HZ_A;

	} else if (bandwidth <= 194) {
		// accel_onchip_filter_bandwith = 194;
		setbits |= REG2_AA_FILTER_BW_194HZ_A;

	} else if (bandwidth <= 362) {
		// accel_onchip_filter_bandwith = 362;
		setbits |= REG2_AA_FILTER_BW_362HZ_A;

	} else if (bandwidth <= 773) {
		// accel_onchip_filter_bandwith = 773;
		setbits |= REG2_AA_FILTER_BW_773HZ_A;

	} else {
		return -EINVAL;
	}

	modify_reg(ADDR_CTRL_REG2, clearbits, setbits);

	return OK;
}

int
LSM303D::accel_set_samplerate(unsigned frequency)
{
	uint8_t setbits = 0;
	uint8_t clearbits = REG1_RATE_BITS_A;

	if (frequency == 0) {
		frequency = 1600;
	}

	int accel_samplerate = 100;

	if (frequency <= 100) {
		setbits |= REG1_RATE_100HZ_A;
		accel_samplerate = 100;

	} else if (frequency <= 200) {
		setbits |= REG1_RATE_200HZ_A;
		accel_samplerate = 200;

	} else if (frequency <= 400) {
		setbits |= REG1_RATE_400HZ_A;
		accel_samplerate = 400;

	} else if (frequency <= 800) {
		setbits |= REG1_RATE_800HZ_A;
		accel_samplerate = 800;

	} else if (frequency <= 1600) {
		setbits |= REG1_RATE_1600HZ_A;
		accel_samplerate = 1600;

	} else {
		return -EINVAL;
	}

	_call_accel_interval = 1000000 / accel_samplerate;
	_px4_accel.set_sample_rate(accel_samplerate);

	modify_reg(ADDR_CTRL_REG1, clearbits, setbits);

	return OK;
}

int
LSM303D::mag_set_samplerate(unsigned frequency)
{
	uint8_t setbits = 0;
	uint8_t clearbits = REG5_RATE_BITS_M;

	if (frequency == 0) {
		frequency = 100;
	}

	int mag_samplerate = 100;

	if (frequency <= 25) {
		setbits |= REG5_RATE_25HZ_M;
		mag_samplerate = 25;

	} else if (frequency <= 50) {
		setbits |= REG5_RATE_50HZ_M;
		mag_samplerate = 50;

	} else if (frequency <= 100) {
		setbits |= REG5_RATE_100HZ_M;
		mag_samplerate = 100;

	} else {
		return -EINVAL;
	}

	_call_mag_interval = 1000000 / mag_samplerate;

	modify_reg(ADDR_CTRL_REG5, clearbits, setbits);

	return OK;
}

void
LSM303D::start()
{
	// make sure we are stopped first
	stop();

	// start polling at the specified rate
	ScheduleOnInterval(_call_accel_interval - LSM303D_TIMER_REDUCTION);
}

void
LSM303D::stop()
{
	ScheduleClear();
}

void
LSM303D::Run()
{
	// make another accel measurement
	measureAccelerometer();

	if (hrt_elapsed_time(&_mag_last_measure) >= _call_mag_interval) {
		measureMagnetometer();
	}
}

void
LSM303D::check_registers(void)
{
	uint8_t v = 0;

	if ((v = read_reg(_checked_registers[_checked_next])) != _checked_values[_checked_next]) {
		/*
		  if we get the wrong value then we know the SPI bus
		  or sensor is very sick. We set _register_wait to 20
		  and wait until we have seen 20 good values in a row
		  before we consider the sensor to be OK again.
		 */
		perf_count(_bad_registers);

		/*
		  try to fix the bad register value. We only try to
		  fix one per loop to prevent a bad sensor hogging the
		  bus. We skip zero as that is the WHO_AM_I, which
		  is not writeable
		 */
		if (_checked_next != 0) {
			write_reg(_checked_registers[_checked_next], _checked_values[_checked_next]);
		}

		_register_wait = 20;
	}

	_checked_next = (_checked_next + 1) % LSM303D_NUM_CHECKED_REGISTERS;
}

void
LSM303D::measureAccelerometer()
{
	perf_begin(_accel_sample_perf);
	perf_count(_accel_sample_interval_perf);

	// status register and data as read back from the device
#pragma pack(push, 1)
	struct {
		uint8_t		cmd;
		uint8_t		status;
		int16_t		x;
		int16_t		y;
		int16_t		z;
	} raw_accel_report{};
#pragma pack(pop)

	check_registers();

	if (_register_wait != 0) {
		// we are waiting for some good transfers before using
		// the sensor again.
		_register_wait--;
		perf_end(_accel_sample_perf);
		return;
	}

	/* fetch data from the sensor */
	const hrt_abstime timestamp_sample = hrt_absolute_time();
	raw_accel_report.cmd = ADDR_STATUS_A | DIR_READ | ADDR_INCREMENT;
	transfer((uint8_t *)&raw_accel_report, (uint8_t *)&raw_accel_report, sizeof(raw_accel_report));

	if (!(raw_accel_report.status & REG_STATUS_A_NEW_ZYXADA)) {
		perf_end(_accel_sample_perf);
		perf_count(_accel_duplicates);
		return;
	}

	/*
	  we have logs where the accelerometers get stuck at a fixed
	  large value. We want to detect this and mark the sensor as
	  being faulty
	 */
	if (((_last_accel[0] - raw_accel_report.x) == 0) &&
	    ((_last_accel[1] - raw_accel_report.y) == 0) &&
	    ((_last_accel[2] - raw_accel_report.z) == 0)) {

		_constant_accel_count++;

	} else {
		_constant_accel_count = 0;
	}

	if (_constant_accel_count > 100) {
		// we've had 100 constant accel readings with large
		// values. The sensor is almost certainly dead. We
		// will raise the error_count so that the top level
		// flight code will know to avoid this sensor, but
		// we'll still give the data so that it can be logged
		// and viewed
		perf_count(_bad_values);

		_constant_accel_count = 0;

		perf_end(_accel_sample_perf);
		return;
	}

	// report the error count as the sum of the number of bad
	// register reads and bad values. This allows the higher level
	// code to decide if it should use this sensor based on
	// whether it has had failures
	_px4_accel.set_error_count(perf_event_count(_bad_registers) + perf_event_count(_bad_values));
	_px4_accel.update(timestamp_sample, raw_accel_report.x, raw_accel_report.y, raw_accel_report.z);

	_last_accel[0] = raw_accel_report.x;
	_last_accel[1] = raw_accel_report.y;
	_last_accel[2] = raw_accel_report.z;

	perf_end(_accel_sample_perf);
}

void
LSM303D::measureMagnetometer()
{
	perf_begin(_mag_sample_perf);
	perf_count(_mag_sample_interval_perf);

	// status register and data as read back from the device
#pragma pack(push, 1)
	struct {
		uint8_t		cmd;
		int16_t		temperature;
		uint8_t		status;
		int16_t		x;
		int16_t		y;
		int16_t		z;
	} raw_mag_report{};
#pragma pack(pop)

	// fetch data from the sensor
	const hrt_abstime timestamp_sample = hrt_absolute_time();
	raw_mag_report.cmd = ADDR_OUT_TEMP_L | DIR_READ | ADDR_INCREMENT;
	transfer((uint8_t *)&raw_mag_report, (uint8_t *)&raw_mag_report, sizeof(raw_mag_report));

	/* remember the temperature. The datasheet isn't clear, but it
	 * seems to be a signed offset from 25 degrees C in units of 0.125C
	 */
	_last_temperature = 25.0f + (raw_mag_report.temperature * 0.125f);
	_px4_accel.set_temperature(_last_temperature);
	_px4_mag.set_temperature(_last_temperature);

	_px4_mag.set_error_count(perf_event_count(_bad_registers) + perf_event_count(_bad_values));
	_px4_mag.set_external(external());
	_px4_mag.update(timestamp_sample, raw_mag_report.x, raw_mag_report.y, raw_mag_report.z);

	_mag_last_measure = timestamp_sample;

	perf_end(_mag_sample_perf);
}

void
LSM303D::print_info()
{
	perf_print_counter(_accel_sample_perf);
	perf_print_counter(_accel_sample_interval_perf);
	perf_print_counter(_mag_sample_perf);
	perf_print_counter(_mag_sample_interval_perf);
	perf_print_counter(_bad_registers);
	perf_print_counter(_bad_values);
	perf_print_counter(_accel_duplicates);

	::printf("checked_next: %u\n", _checked_next);

	for (uint8_t i = 0; i < LSM303D_NUM_CHECKED_REGISTERS; i++) {
		uint8_t v = read_reg(_checked_registers[i]);

		if (v != _checked_values[i]) {
			::printf("reg %02x:%02x should be %02x\n",
				 (unsigned)_checked_registers[i],
				 (unsigned)v,
				 (unsigned)_checked_values[i]);
		}
	}
}

/**
 * Local functions in support of the shell command.
 */
namespace lsm303d
{

LSM303D	*g_dev;

void	start(bool external_bus, enum Rotation rotation, unsigned range);
void	info();
void	usage();

/**
 * Start the driver.
 *
 * This function call only returns once the driver is
 * up and running or failed to detect the sensor.
 */
void
start(bool external_bus, enum Rotation rotation, unsigned range)
{
	if (g_dev != nullptr) {
		errx(0, "already started");
	}

	/* create the driver */
	if (external_bus) {
#if defined(PX4_SPI_BUS_EXT) && defined(PX4_SPIDEV_EXT_ACCEL_MAG)
		g_dev = new LSM303D(PX4_SPI_BUS_EXT, PX4_SPIDEV_EXT_ACCEL_MAG, rotation);
#else
		errx(0, "External SPI not available");
#endif

	} else {
		g_dev = new LSM303D(PX4_SPI_BUS_SENSORS, PX4_SPIDEV_ACCEL_MAG, rotation);
	}

	if (g_dev == nullptr) {
		PX4_ERR("failed instantiating LSM303D obj");
		goto fail;
	}

	if (OK != g_dev->init()) {
		goto fail;
	}

	exit(0);
fail:

	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;
	}

	errx(1, "driver start failed");
}

/**
 * Print a little info about the driver.
 */
void
info()
{
	if (g_dev == nullptr) {
		errx(1, "driver not running\n");
	}

	printf("state @ %p\n", g_dev);
	g_dev->print_info();

	exit(0);
}

void
usage()
{
	PX4_INFO("missing command: try 'start', 'info'");
	PX4_INFO("options:");
	PX4_INFO("    -X    (external bus)");
	PX4_INFO("    -R rotation");
}

} // namespace

int
lsm303d_main(int argc, char *argv[])
{
	bool external_bus = false;
	enum Rotation rotation = ROTATION_NONE;
	int accel_range = 8;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	/* jump over start/off/etc and look at options first */
	while ((ch = px4_getopt(argc, argv, "XR:a:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'X':
			external_bus = true;
			break;

		case 'R':
			rotation = (enum Rotation)atoi(myoptarg);
			break;

		case 'a':
			accel_range = atoi(myoptarg);
			break;

		default:
			lsm303d::usage();
			exit(0);
		}
	}

	if (myoptind >= argc) {
		lsm303d::usage();
		exit(0);
	}

	const char *verb = argv[myoptind];

	/*
	 * Start/load the driver.

	 */
	if (!strcmp(verb, "start")) {
		lsm303d::start(external_bus, rotation, accel_range);
	}

	/*
	 * Print driver information.
	 */
	if (!strcmp(verb, "info")) {
		lsm303d::info();
	}

	errx(1, "unrecognized command, try 'start', info'");
}
