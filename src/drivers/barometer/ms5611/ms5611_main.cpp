/****************************************************************************
 *
 *   Copyright (c) 2012-2019, 2021 PX4 Development Team. All rights reserved.
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

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <px4_platform_common/module.h>


#include "MS5611.hpp"
#include "ms5611.h"

I2CSPIDriverBase *MS5611::instantiate(const I2CSPIDriverConfig &config, int runtime_instance)
{
	ms5611::prom_u prom_buf;
	device::Device *interface = nullptr;

#if defined(CONFIG_I2C)

	if (config.bus_type == BOARD_I2C_BUS) {
		interface = MS5611_i2c_interface(prom_buf, config.spi_devid, config.bus, config.bus_frequency);

	} else
#endif // CONFIG_I2C
		if (config.bus_type == BOARD_SPI_BUS) {
			interface = MS5611_spi_interface(prom_buf, config.spi_devid, config.bus, config.bus_frequency, config.spi_mode);
		}

	if (interface == nullptr) {
		PX4_ERR("alloc failed");
		return nullptr;
	}

	if (interface->init() != OK) {
		delete interface;
		PX4_DEBUG("no device on bus %i (devid 0x%x)", config.bus, config.spi_devid);
		return nullptr;
	}

	MS5611 *dev = new MS5611(interface, prom_buf, config);

	if (dev == nullptr) {
		delete interface;
		return nullptr;
	}

	if (OK != dev->init()) {
		delete dev;
		return nullptr;
	}

	return dev;
}

void MS5611::print_usage()
{
	PRINT_MODULE_USAGE_NAME("ms5611", "driver");
	PRINT_MODULE_USAGE_SUBCATEGORY("baro");
	PRINT_MODULE_USAGE_COMMAND("start");
#if defined(CONFIG_I2C)
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, true);
#else
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(false, true);
#endif
	PRINT_MODULE_USAGE_PARAM_STRING('T', "5611", "5607|5611", "Device type", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" int ms5611_main(int argc, char *argv[])
{
	using ThisDriver = MS5611;
	int ch;
#if defined(CONFIG_I2C)
	BusCLIArguments cli {true, true};
	cli.default_i2c_frequency = 400000;
	cli.i2c_address = MS5611_ADDRESS_1;
#else
	BusCLIArguments cli {false, true};
#endif
	cli.default_spi_frequency = 16 * 1000 * 1000;
	uint16_t dev_type_driver = DRV_BARO_DEVTYPE_MS5611;

	while ((ch = cli.getOpt(argc, argv, "T:")) != EOF) {
		switch (ch) {
		case 'T': {
				int val = atoi(cli.optArg());

				if (val == 5611) {
					dev_type_driver = DRV_BARO_DEVTYPE_MS5611;

				} else if (val == 5607) {
					dev_type_driver = DRV_BARO_DEVTYPE_MS5607;
				}
			}
			break;
		}
	}

	const char *verb = cli.optArg();

	if (!verb) {
		ThisDriver::print_usage();
		return -1;
	}

	BusInstanceIterator iterator(MODULE_NAME, cli, dev_type_driver);

	if (!strcmp(verb, "start")) {
		return ThisDriver::module_start(cli, iterator);
	}

	if (!strcmp(verb, "stop")) {
		return ThisDriver::module_stop(iterator);
	}

	if (!strcmp(verb, "status")) {
		return ThisDriver::module_status(iterator);
	}

	ThisDriver::print_usage();
	return -1;
}
