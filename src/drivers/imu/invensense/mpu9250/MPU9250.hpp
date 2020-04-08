/****************************************************************************
 *
 *   Copyright (c) 2020 PX4 Development Team. All rights reserved.
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
 * @file MPU9250.hpp
 *
 * Driver for the Invensense MPU9250 connected via SPI.
 *
 */

#pragma once

#include "InvenSense_MPU9250_registers.hpp"

#include <drivers/drv_hrt.h>
#include <lib/drivers/accelerometer/PX4Accelerometer.hpp>
#include <lib/drivers/device/spi.h>
#include <lib/drivers/gyroscope/PX4Gyroscope.hpp>
#include <lib/ecl/geo/geo.h>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/atomic.h>
#include <px4_platform_common/i2c_spi_buses.h>

#include "MPU9250_AK8963.hpp"

using namespace InvenSense_MPU9250;

class MPU9250 : public device::SPI, public I2CSPIDriver<MPU9250>
{
public:
	MPU9250(I2CSPIBusOption bus_option, int bus, uint32_t device, enum Rotation rotation, int bus_frequency,
		spi_mode_e spi_mode, spi_drdy_gpio_t drdy_gpio, bool enable_magnetometer = false);
	~MPU9250() override;

	static I2CSPIDriverBase *instantiate(const BusCLIArguments &cli, const BusInstanceIterator &iterator,
					     int runtime_instance);
	static void print_usage();

	void RunImpl();

	int init() override;
	void print_status() override;

private:
	void exit_and_cleanup() override;

	// Sensor Configuration
	static constexpr float FIFO_SAMPLE_DT{125.f};
	static constexpr uint32_t SAMPLES_PER_TRANSFER{2};       // ensure at least 1 new accel sample per transfer
	static constexpr float GYRO_RATE{1e6f / FIFO_SAMPLE_DT}; // 8 kHz gyro
	static constexpr float ACCEL_RATE{GYRO_RATE / 2.f};      // 4 kHz accel

	static constexpr uint32_t FIFO_MAX_SAMPLES{math::min(FIFO::SIZE / sizeof(FIFO::DATA), sizeof(PX4Gyroscope::FIFOSample::x) / sizeof(PX4Gyroscope::FIFOSample::x[0]))};

	// Transfer data
	struct FIFOTransferBuffer {
		uint8_t cmd{static_cast<uint8_t>(Register::FIFO_R_W) | DIR_READ};
		FIFO::DATA f[FIFO_MAX_SAMPLES] {};
	};
	// ensure no struct padding
	static_assert(sizeof(FIFOTransferBuffer) == (1 + FIFO_MAX_SAMPLES *sizeof(FIFO::DATA)));

	struct register_config_t {
		Register reg;
		uint8_t set_bits{0};
		uint8_t clear_bits{0};
	};

	int probe() override;

	bool Reset();

	bool Configure();
	void ConfigureAccel();
	void ConfigureGyro();
	void ConfigureSampleRate(int sample_rate);

	static int DataReadyInterruptCallback(int irq, void *context, void *arg);
	void DataReady();
	bool DataReadyInterruptConfigure();
	bool DataReadyInterruptDisable();

	bool RegisterCheck(const register_config_t &reg_cfg, bool notify = false);

	uint8_t RegisterRead(Register reg);
	void RegisterWrite(Register reg, uint8_t value);
	void RegisterSetAndClearBits(Register reg, uint8_t setbits, uint8_t clearbits);
	void RegisterSetBits(Register reg, uint8_t setbits) { RegisterSetAndClearBits(reg, setbits, 0); }
	void RegisterClearBits(Register reg, uint8_t clearbits) { RegisterSetAndClearBits(reg, 0, clearbits); }

	uint16_t FIFOReadCount();
	bool FIFORead(const hrt_abstime &timestamp_sample, uint16_t samples);
	void FIFOReset();

	bool ProcessAccel(const hrt_abstime &timestamp_sample, const FIFOTransferBuffer &buffer, const uint8_t samples);
	void ProcessGyro(const hrt_abstime &timestamp_sample, const FIFOTransferBuffer &buffer, const uint8_t samples);
	void UpdateTemperature();

	const spi_drdy_gpio_t _drdy_gpio;

	// I2C AUX interface (slave 1 - 4)
	friend class AKM_AK8963::MPU9250_AK8963;

	void I2CSlaveRegisterStartRead(uint8_t slave_i2c_addr, uint8_t reg);
	void I2CSlaveRegisterWrite(uint8_t slave_i2c_addr, uint8_t reg, uint8_t val);
	void I2CSlaveExternalSensorDataEnable(uint8_t slave_i2c_addr, uint8_t reg, uint8_t size);
	bool I2CSlaveExternalSensorDataRead(uint8_t *buffer, uint8_t length);

	AKM_AK8963::MPU9250_AK8963 *_slave_ak8963_magnetometer{nullptr};

	PX4Accelerometer _px4_accel;
	PX4Gyroscope _px4_gyro;

	perf_counter_t _transfer_perf{perf_alloc(PC_ELAPSED, MODULE_NAME": transfer")};
	perf_counter_t _bad_register_perf{perf_alloc(PC_COUNT, MODULE_NAME": bad register")};
	perf_counter_t _bad_transfer_perf{perf_alloc(PC_COUNT, MODULE_NAME": bad transfer")};
	perf_counter_t _fifo_empty_perf{perf_alloc(PC_COUNT, MODULE_NAME": FIFO empty")};
	perf_counter_t _fifo_overflow_perf{perf_alloc(PC_COUNT, MODULE_NAME": FIFO overflow")};
	perf_counter_t _fifo_reset_perf{perf_alloc(PC_COUNT, MODULE_NAME": FIFO reset")};
	perf_counter_t _drdy_interval_perf{perf_alloc(PC_INTERVAL, MODULE_NAME": DRDY interval")};

	hrt_abstime _reset_timestamp{0};
	hrt_abstime _last_config_check_timestamp{0};
	hrt_abstime _fifo_watermark_interrupt_timestamp{0};
	hrt_abstime _temperature_update_timestamp{0};

	px4::atomic<uint8_t> _data_ready_count{0};
	px4::atomic<uint8_t> _fifo_read_samples{0};
	bool _data_ready_interrupt_enabled{false};

	enum class STATE : uint8_t {
		RESET,
		WAIT_FOR_RESET,
		CONFIGURE,
		FIFO_READ,
	};

	STATE _state{STATE::RESET};

	uint16_t _fifo_empty_interval_us{1000}; // default 1000 us / 1000 Hz transfer interval
	uint8_t _fifo_gyro_samples{static_cast<uint8_t>(_fifo_empty_interval_us / (1000000 / GYRO_RATE))};
	uint8_t _fifo_accel_samples{static_cast<uint8_t>(_fifo_empty_interval_us / (1000000 / ACCEL_RATE))};

	uint8_t _checked_register{0};
	static constexpr uint8_t size_register_cfg{12};
	register_config_t _register_cfg[size_register_cfg] {
		// Register                     | Set bits, Clear bits
		{ Register::PWR_MGMT_1,         PWR_MGMT_1_BIT::CLKSEL_0, PWR_MGMT_1_BIT::H_RESET | PWR_MGMT_1_BIT::SLEEP },
		{ Register::ACCEL_CONFIG,       ACCEL_CONFIG_BIT::ACCEL_FS_SEL_16G, 0 },
		{ Register::ACCEL_CONFIG2,      ACCEL_CONFIG2_BIT::ACCEL_FCHOICE_B_BYPASS_DLPF, 0 },
		{ Register::GYRO_CONFIG,        GYRO_CONFIG_BIT::GYRO_FS_SEL_2000_DPS, GYRO_CONFIG_BIT::FCHOICE_B_8KHZ_BYPASS_DLPF },
		{ Register::CONFIG,             CONFIG_BIT::FIFO_MODE | CONFIG_BIT::DLPF_CFG_BYPASS_DLPF_8KHZ, 0 },
		{ Register::USER_CTRL,          USER_CTRL_BIT::FIFO_EN | USER_CTRL_BIT::I2C_MST_EN | USER_CTRL_BIT::I2C_IF_DIS, 0 },
		{ Register::FIFO_EN,            FIFO_EN_BIT::GYRO_XOUT | FIFO_EN_BIT::GYRO_YOUT | FIFO_EN_BIT::GYRO_ZOUT | FIFO_EN_BIT::ACCEL, 0 },
		{ Register::I2C_SLV4_CTRL,      I2C_SLV4_CTRL_BIT::I2C_MST_DLY, 0 },
		{ Register::I2C_MST_CTRL,       I2C_MST_CTRL_BIT::I2C_MST_P_NSR | I2C_MST_CTRL_BIT::I2C_MST_CLK_400_kHz, 0 },
		{ Register::I2C_MST_DELAY_CTRL, I2C_MST_DELAY_CTRL_BIT::I2C_SLVX_DLY_EN, 0 },
		{ Register::INT_PIN_CFG,        INT_PIN_CFG_BIT::ACTL, 0 },
		{ Register::INT_ENABLE,         INT_ENABLE_BIT::RAW_RDY_EN, 0 }
	};
};
