/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
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

#include "ICM42688P.hpp"

#include <px4_platform/board_dma_alloc.h>

using namespace time_literals;

static constexpr int16_t combine(uint8_t msb, uint8_t lsb)
{
	return (msb << 8u) | lsb;
}

ICM42688P::ICM42688P(int bus, uint32_t device, enum Rotation rotation) :
	SPI(MODULE_NAME, nullptr, bus, device, SPIDEV_MODE3, SPI_SPEED),
	ScheduledWorkItem(MODULE_NAME, px4::device_bus_to_wq(get_device_id())),
	_px4_accel(get_device_id(), ORB_PRIO_VERY_HIGH, rotation),
	_px4_gyro(get_device_id(), ORB_PRIO_VERY_HIGH, rotation)
{
	set_device_type(DRV_IMU_DEVTYPE_ICM42688P);

	_px4_accel.set_device_type(DRV_IMU_DEVTYPE_ICM42688P);
	_px4_gyro.set_device_type(DRV_IMU_DEVTYPE_ICM42688P);

	ConfigureSampleRate(_px4_gyro.get_max_rate_hz());
}

ICM42688P::~ICM42688P()
{
	Stop();

	if (_dma_data_buffer != nullptr) {
		board_dma_free(_dma_data_buffer, FIFO::SIZE);
	}

	perf_free(_transfer_perf);
	perf_free(_bad_register_perf);
	perf_free(_bad_transfer_perf);
	perf_free(_fifo_empty_perf);
	perf_free(_fifo_overflow_perf);
	perf_free(_fifo_reset_perf);
	perf_free(_drdy_interval_perf);
}

bool ICM42688P::Init()
{
	if (SPI::init() != PX4_OK) {
		PX4_ERR("SPI::init failed");
		return false;
	}

	// allocate DMA capable buffer
	_dma_data_buffer = (uint8_t *)board_dma_alloc(FIFO::SIZE);

	if (_dma_data_buffer == nullptr) {
		PX4_ERR("DMA alloc failed");
		return false;
	}

	return Reset();
}

void ICM42688P::Stop()
{
	// wait until stopped
	while (_state.load() != STATE::STOPPED) {
		_state.store(STATE::REQUEST_STOP);
		ScheduleNow();
		px4_usleep(10);
	}
}

bool ICM42688P::Reset()
{
	_state.store(STATE::RESET);
	ScheduleClear();
	ScheduleNow();
	return true;
}

void ICM42688P::PrintInfo()
{
	PX4_INFO("FIFO empty interval: %d us (%.3f Hz)", _fifo_empty_interval_us,
		 static_cast<double>(1000000 / _fifo_empty_interval_us));

	perf_print_counter(_transfer_perf);
	perf_print_counter(_bad_register_perf);
	perf_print_counter(_bad_transfer_perf);
	perf_print_counter(_fifo_empty_perf);
	perf_print_counter(_fifo_overflow_perf);
	perf_print_counter(_fifo_reset_perf);
	perf_print_counter(_drdy_interval_perf);

	_px4_accel.print_status();
	_px4_gyro.print_status();
}

int ICM42688P::probe()
{
	const uint8_t whoami = RegisterRead(Register::BANK_0::WHO_AM_I);

	if (whoami != WHOAMI) {
		PX4_WARN("unexpected WHO_AM_I 0x%02x", whoami);
		return PX4_ERROR;
	}

	return PX4_OK;
}

void ICM42688P::Run()
{
	switch (_state.load()) {
	case STATE::RESET:
		// DEVICE_CONFIG: Software reset
		RegisterWrite(Register::BANK_0::DEVICE_CONFIG, DEVICE_CONFIG_BIT::SOFT_RESET_CONFIG);
		_reset_timestamp = hrt_absolute_time();
		_state.store(STATE::WAIT_FOR_RESET);
		ScheduleDelayed(1_ms); // wait 1 ms for soft reset to be effective
		break;

	case STATE::WAIT_FOR_RESET:
		if ((RegisterRead(Register::BANK_0::WHO_AM_I) == WHOAMI)
		    && (RegisterRead(Register::BANK_0::INT_STATUS) & INT_STATUS_BIT::RESET_DONE_INT)) {
			// if reset succeeded then configure
			_state.store(STATE::CONFIGURE);
			ScheduleNow();

		} else {
			// RESET not complete
			if (hrt_elapsed_time(&_reset_timestamp) > 10_ms) {
				PX4_ERR("Reset failed, retrying");
				_state.store(STATE::RESET);
				ScheduleDelayed(10_ms);

			} else {
				PX4_DEBUG("Reset not complete, check again in 10 ms");
				ScheduleDelayed(10_ms);
			}
		}

		break;

	case STATE::CONFIGURE:
		if (Configure()) {
			// if configure succeeded then start reading from FIFO
			_state.store(STATE::FIFO_READ);

			if (DataReadyInterruptConfigure()) {
				_data_ready_interrupt_enabled = true;

				// backup schedule as a watchdog timeout
				ScheduleDelayed(10_ms);

			} else {
				_data_ready_interrupt_enabled = false;
				ScheduleOnInterval(_fifo_empty_interval_us, _fifo_empty_interval_us);
			}

			FIFOReset();

		} else {
			PX4_DEBUG("Configure failed, retrying");
			// try again in 10 ms
			ScheduleDelayed(10_ms);
		}

		break;

	case STATE::FIFO_READ: {
			hrt_abstime timestamp_sample = 0;
			uint8_t samples = 0;

			if (_data_ready_interrupt_enabled) {
				// re-schedule as watchdog timeout
				ScheduleDelayed(10_ms);

				// timestamp set in data ready interrupt
				samples = _fifo_read_samples.load();
				timestamp_sample = _fifo_watermark_interrupt_timestamp;
			}

			bool failure = false;

			// manually check FIFO count if no samples from DRDY or timestamp looks bogus
			if (!_data_ready_interrupt_enabled || (samples == 0)
			    || (hrt_elapsed_time(&timestamp_sample) > (_fifo_empty_interval_us / 2))) {

				// use the time now roughly corresponding with the last sample we'll pull from the FIFO
				timestamp_sample = hrt_absolute_time();
				const uint16_t fifo_count = FIFOReadCount();

				if (fifo_count == 0) {
					failure = true;
					perf_count(_fifo_empty_perf);
				}

				samples = fifo_count / sizeof(FIFO::DATA);
			}

			if (samples > FIFO_MAX_SAMPLES) {
				// not technically an overflow, but more samples than we expected or can publish
				perf_count(_fifo_overflow_perf);
				failure = true;
				FIFOReset();

			} else if (samples >= 1) {
				if (!FIFORead(timestamp_sample, samples)) {
					failure = true;
					_px4_accel.increase_error_count();
					_px4_gyro.increase_error_count();
				}
			}

			if (failure || hrt_elapsed_time(&_last_config_check_timestamp) > 10_ms) {
				// check BANK_0 registers incrementally
				if (RegisterCheck(_register_bank0_cfg[_checked_register_bank0], true)) {

					_last_config_check_timestamp = timestamp_sample;
					_checked_register_bank0 = (_checked_register_bank0 + 1) % size_register_bank0_cfg;

				} else {
					// register check failed, force reconfigure
					PX4_DEBUG("Health check failed, reconfiguring");
					_state.store(STATE::CONFIGURE);
					ScheduleNow();
				}

			} else {
				// periodically update temperature (1 Hz)
				if (hrt_elapsed_time(&_temperature_update_timestamp) > 1_s) {
					UpdateTemperature();
					_temperature_update_timestamp = timestamp_sample;
				}
			}
		}

		break;

	case STATE::REQUEST_STOP:
		DataReadyInterruptDisable();
		ScheduleClear();
		_state.store(STATE::STOPPED);
		break;

	case STATE::STOPPED:
		// DO NOTHING
		break;
	}
}

void ICM42688P::ConfigureAccel()
{
	const uint8_t ACCEL_FS_SEL = RegisterRead(Register::BANK_0::ACCEL_CONFIG0) & (Bit7 | Bit6 | Bit5); // 7:5 ACCEL_FS_SEL

	switch (ACCEL_FS_SEL) {
	case ACCEL_FS_SEL_2G:
		_px4_accel.set_scale(CONSTANTS_ONE_G / 16384);
		_px4_accel.set_range(2 * CONSTANTS_ONE_G);
		break;

	case ACCEL_FS_SEL_4G:
		_px4_accel.set_scale(CONSTANTS_ONE_G / 8192);
		_px4_accel.set_range(4 * CONSTANTS_ONE_G);
		break;

	case ACCEL_FS_SEL_8G:
		_px4_accel.set_scale(CONSTANTS_ONE_G / 4096);
		_px4_accel.set_range(8 * CONSTANTS_ONE_G);
		break;

	case ACCEL_FS_SEL_16G:
		_px4_accel.set_scale(CONSTANTS_ONE_G / 2048);
		_px4_accel.set_range(16 * CONSTANTS_ONE_G);
		break;
	}
}

void ICM42688P::ConfigureGyro()
{
	const uint8_t GYRO_FS_SEL = RegisterRead(Register::BANK_0::GYRO_CONFIG0) & (Bit7 | Bit6 | Bit5); // 7:5 GYRO_FS_SEL

	switch (GYRO_FS_SEL) {
	case GYRO_FS_SEL_125_DPS:
		_px4_gyro.set_scale(math::radians(1.0f / 262.f));
		_px4_gyro.set_range(math::radians(125.f));
		break;

	case GYRO_FS_SEL_250_DPS:
		_px4_gyro.set_scale(math::radians(1.f / 131.f));
		_px4_gyro.set_range(math::radians(250.f));
		break;

	case GYRO_FS_SEL_500_DPS:
		_px4_gyro.set_scale(math::radians(1.f / 65.5f));
		_px4_gyro.set_range(math::radians(500.f));
		break;

	case GYRO_FS_SEL_1000_DPS:
		_px4_gyro.set_scale(math::radians(1.f / 32.8f));
		_px4_gyro.set_range(math::radians(1000.f));
		break;

	case GYRO_FS_SEL_2000_DPS:
		_px4_gyro.set_scale(math::radians(1.f / 16.4f));
		_px4_gyro.set_range(math::radians(2000.0f));
		break;
	}
}

void ICM42688P::ConfigureSampleRate(int sample_rate)
{
	if (sample_rate == 0) {
		sample_rate = 2000; // default to 2 kHz
	}

	static constexpr float sample_min_interval = 1e6f / GYRO_RATE;

	_fifo_empty_interval_us = math::max(((1000000 / sample_rate) / sample_min_interval) * sample_min_interval,
					    sample_min_interval); // round down to nearest sample_min_interval

	_fifo_gyro_samples = math::min(_fifo_empty_interval_us / (1000000 / GYRO_RATE), FIFO_MAX_SAMPLES);

	// recompute FIFO empty interval (us) with actual gyro sample limit
	_fifo_empty_interval_us = _fifo_gyro_samples * (1000000 / GYRO_RATE);

	_fifo_accel_samples = math::min(_fifo_empty_interval_us / (1000000 / ACCEL_RATE), FIFO_MAX_SAMPLES);

	_px4_accel.set_update_rate(1000000 / _fifo_empty_interval_us);
	_px4_gyro.set_update_rate(1000000 / _fifo_empty_interval_us);

	// FIFO watermark threshold in number of bytes
	const uint16_t fifo_watermark_threshold = _fifo_gyro_samples * sizeof(FIFO::DATA);

	for (auto &r : _register_bank0_cfg) {
		if (r.reg == Register::BANK_0::FIFO_CONFIG2) {
			// FIFO_WM[7:0]  FIFO_CONFIG2
			r.set_bits = fifo_watermark_threshold & 0xFF;

		} else if (r.reg == Register::BANK_0::FIFO_CONFIG3) {
			// FIFO_WM[11:8] FIFO_CONFIG3
			r.set_bits = (fifo_watermark_threshold >> 8) & 0x0F;
		}
	}
}

bool ICM42688P::Configure()
{
	bool success = true;

	for (const auto &reg : _register_bank0_cfg) {
		if (!RegisterCheck(reg)) {
			success = false;
		}
	}

	ConfigureAccel();
	ConfigureGyro();

	return success;
}

int ICM42688P::DataReadyInterruptCallback(int irq, void *context, void *arg)
{
	static_cast<ICM42688P *>(arg)->DataReady();
	return 0;
}

void ICM42688P::DataReady()
{
	_fifo_watermark_interrupt_timestamp = hrt_absolute_time();
	_fifo_read_samples.store(_fifo_gyro_samples);
	ScheduleNow();
	perf_count(_drdy_interval_perf);
}

bool ICM42688P::DataReadyInterruptConfigure()
{
	int ret_setevent = -1;

	// Setup data ready on falling edge
	// TODO: cleanup horrible DRDY define mess
#if defined(GPIO_SPI2_DRDY1_ICM_42688)
	ret_setevent = px4_arch_gpiosetevent(GPIO_SPI2_DRDY1_ICM_42688, false, true, true,
					     &ICM42688P::DataReadyInterruptCallback, this);
#endif

	return (ret_setevent == 0);
}

bool ICM42688P::DataReadyInterruptDisable()
{
	int ret_setevent = -1;

	// Disable data ready callback
	// TODO: cleanup horrible DRDY define mess
#if defined(GPIO_SPI2_DRDY1_ICM_42688)
	ret_setevent = px4_arch_gpiosetevent(GPIO_SPI2_DRDY1_ICM_42688, false, false, false, nullptr, nullptr);
#endif

	return (ret_setevent == 0);
}

bool ICM42688P::RegisterCheck(const register_bank0_config_t &reg_cfg, bool notify)
{
	const uint8_t reg_value = RegisterRead(reg_cfg.reg);
	bool success = true;

	if (reg_cfg.set_bits && ((reg_value & reg_cfg.set_bits) != reg_cfg.set_bits)) {
		PX4_DEBUG("0x%02hhX: 0x%02hhX (0x%02hhX not set)", (uint8_t)reg_cfg.reg, reg_value, reg_cfg.set_bits);
		success = false;
	}

	if (reg_cfg.clear_bits && ((reg_value & reg_cfg.clear_bits) != 0)) {
		PX4_DEBUG("0x%02hhX: 0x%02hhX (0x%02hhX not cleared)", (uint8_t)reg_cfg.reg, reg_value, reg_cfg.clear_bits);
		success = false;
	}

	if (!success) {
		RegisterSetAndClearBits(reg_cfg.reg, reg_cfg.set_bits, reg_cfg.clear_bits);

		if (notify) {
			perf_count(_bad_register_perf);
			_px4_accel.increase_error_count();
			_px4_gyro.increase_error_count();
		}
	}

	return success;
}

uint8_t ICM42688P::RegisterRead(Register::BANK_0 reg)
{
	uint8_t cmd[2] {};
	cmd[0] = static_cast<uint8_t>(reg) | DIR_READ;
	transfer(cmd, cmd, sizeof(cmd));
	return cmd[1];
}

void ICM42688P::RegisterWrite(Register::BANK_0 reg, uint8_t value)
{
	uint8_t cmd[2] { (uint8_t)reg, value };
	transfer(cmd, cmd, sizeof(cmd));
}

void ICM42688P::RegisterSetAndClearBits(Register::BANK_0 reg, uint8_t setbits, uint8_t clearbits)
{
	const uint8_t orig_val = RegisterRead(reg);
	uint8_t val = orig_val;

	if (setbits) {
		val |= setbits;
	}

	if (clearbits) {
		val &= ~clearbits;
	}

	RegisterWrite(reg, val);
}

void ICM42688P::RegisterSetBits(Register::BANK_0 reg, uint8_t setbits)
{
	RegisterSetAndClearBits(reg, setbits, 0);
}

void ICM42688P::RegisterClearBits(Register::BANK_0 reg, uint8_t clearbits)
{
	RegisterSetAndClearBits(reg, 0, clearbits);
}

uint16_t ICM42688P::FIFOReadCount()
{
	// read FIFO count
	uint8_t fifo_count_buf[3] {};
	fifo_count_buf[0] = static_cast<uint8_t>(Register::BANK_0::FIFO_COUNTH) | DIR_READ;

	if (transfer(fifo_count_buf, fifo_count_buf, sizeof(fifo_count_buf)) != PX4_OK) {
		perf_count(_bad_transfer_perf);
		return 0;
	}

	return combine(fifo_count_buf[1], fifo_count_buf[2]);
}

bool ICM42688P::FIFORead(const hrt_abstime &timestamp_sample, uint16_t samples)
{
	TransferBuffer *report = (TransferBuffer *)_dma_data_buffer;
	const size_t transfer_size = math::min(samples * sizeof(FIFO::DATA) + 4, FIFO::SIZE);
	memset(report, 0, transfer_size);
	report->cmd = static_cast<uint8_t>(Register::BANK_0::INT_STATUS) | DIR_READ;

	perf_begin(_transfer_perf);

	if (transfer(_dma_data_buffer, _dma_data_buffer, transfer_size) != PX4_OK) {
		perf_end(_transfer_perf);
		perf_count(_bad_transfer_perf);
		return false;
	}

	perf_end(_transfer_perf);

	if (report->INT_STATUS & INT_STATUS_BIT::FIFO_FULL_INT) {
		perf_count(_fifo_overflow_perf);
		FIFOReset();
	}

	const uint16_t fifo_count_bytes = combine(report->FIFO_COUNTH, report->FIFO_COUNTL);
	const uint16_t fifo_count_samples = fifo_count_bytes / sizeof(FIFO::DATA);

	if (fifo_count_samples == 0) {
		perf_count(_fifo_empty_perf);
		return false;
	}

	// check FIFO header in every sample
	int valid_samples = 0;

	for (int i = 0; i < math::min(samples, fifo_count_samples); i++) {
		bool valid = true;

		// With FIFO_ACCEL_EN and FIFO_GYRO_EN header should be 8’b_0110_10xx
		const uint8_t FIFO_HEADER = report->f[i].FIFO_Header;

		if (FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_MSG) {
			// FIFO sample empty if HEADER_MSG set
			valid = false;

		} else if (!(FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_ACCEL)) {
			// accel bit not set
			valid = false;

		} else if (!(FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_GYRO)) {
			// gyro bit not set
			valid = false;
		}

		if (valid) {
			valid_samples++;

		} else {
			perf_count(_bad_transfer_perf);
			break;
		}
	}

	if (valid_samples > 0) {
		ProcessGyro(timestamp_sample, report, valid_samples);
		ProcessAccel(timestamp_sample, report, valid_samples);
		return true;
	}

	return false;
}

void ICM42688P::FIFOReset()
{
	perf_count(_fifo_reset_perf);

	// SIGNAL_PATH_RESET: FIFO flush
	RegisterSetBits(Register::BANK_0::SIGNAL_PATH_RESET, SIGNAL_PATH_RESET_BIT::FIFO_FLUSH);

	// reset while FIFO is disabled
	_data_ready_count.store(0);
	_fifo_watermark_interrupt_timestamp = 0;
	_fifo_read_samples.store(0);
}

void ICM42688P::ProcessAccel(const hrt_abstime &timestamp_sample, const TransferBuffer *const report, uint8_t samples)
{
	PX4Accelerometer::FIFOSample accel;
	accel.timestamp_sample = timestamp_sample;
	accel.dt = _fifo_empty_interval_us / _fifo_accel_samples;

	int accel_samples = 0;

	for (int i = 0; i < samples; i++) {
		const FIFO::DATA &fifo_sample = report->f[i];
		int16_t accel_x = combine(fifo_sample.ACCEL_DATA_X1, fifo_sample.ACCEL_DATA_X0);
		int16_t accel_y = combine(fifo_sample.ACCEL_DATA_Y1, fifo_sample.ACCEL_DATA_Y0);
		int16_t accel_z = combine(fifo_sample.ACCEL_DATA_Z1, fifo_sample.ACCEL_DATA_Z0);

		// sensor's frame is +x forward, +y left, +z up
		//  flip y & z to publish right handed with z down (x forward, y right, z down)
		accel.x[accel_samples] = accel_x;
		accel.y[accel_samples] = (accel_y == INT16_MIN) ? INT16_MAX : -accel_y;
		accel.z[accel_samples] = (accel_z == INT16_MIN) ? INT16_MAX : -accel_z;
		accel_samples++;
	}

	accel.samples = accel_samples;

	_px4_accel.updateFIFO(accel);
}

void ICM42688P::ProcessGyro(const hrt_abstime &timestamp_sample, const TransferBuffer *const report, uint8_t samples)
{
	PX4Gyroscope::FIFOSample gyro;
	gyro.timestamp_sample = timestamp_sample;
	gyro.samples = samples;
	gyro.dt = _fifo_empty_interval_us / _fifo_gyro_samples;

	for (int i = 0; i < samples; i++) {
		const FIFO::DATA &fifo_sample = report->f[i];

		const int16_t gyro_x = combine(fifo_sample.GYRO_DATA_X1, fifo_sample.GYRO_DATA_X0);
		const int16_t gyro_y = combine(fifo_sample.GYRO_DATA_Y1, fifo_sample.GYRO_DATA_Y0);
		const int16_t gyro_z = combine(fifo_sample.GYRO_DATA_Z1, fifo_sample.GYRO_DATA_Z0);

		// sensor's frame is +x forward, +y left, +z up
		//  flip y & z to publish right handed with z down (x forward, y right, z down)
		gyro.x[i] = gyro_x;
		gyro.y[i] = (gyro_y == INT16_MIN) ? INT16_MAX : -gyro_y;
		gyro.z[i] = (gyro_z == INT16_MIN) ? INT16_MAX : -gyro_z;
	}

	_px4_gyro.updateFIFO(gyro);
}

void ICM42688P::UpdateTemperature()
{
	// read current temperature
	uint8_t temperature_buf[3] {};
	temperature_buf[0] = static_cast<uint8_t>(Register::BANK_0::TEMP_DATA1) | DIR_READ;

	if (transfer(temperature_buf, temperature_buf, sizeof(temperature_buf)) != PX4_OK) {
		perf_count(_bad_transfer_perf);
		return;
	}

	const int16_t TEMP_DATA = combine(temperature_buf[1], temperature_buf[2]);

	// Temperature in Degrees Centigrade = (TEMP_DATA / 132.48) + 25
	const float TEMP_degC = (TEMP_DATA / TEMPERATURE_SENSITIVITY) + ROOM_TEMPERATURE_OFFSET;

	if (PX4_ISFINITE(TEMP_degC)) {
		_px4_accel.set_temperature(TEMP_degC);
		_px4_gyro.set_temperature(TEMP_degC);
	}
}
