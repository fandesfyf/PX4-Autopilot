/****************************************************************************
 *
 *   Copyright (c) 2013 PX4 Development Team. All rights reserved.
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
  * @file ms5611_spi.cpp
  *
  * SPI interface for MS5611
  */


/* SPI protocol address bits */
#define DIR_READ			(1<<7)
#define DIR_WRITE			(0<<7)
#define ADDR_INCREMENT			(1<<6)

device::Device *MS5611_spi_interface();

class MS5611_SPI : device::SPI
{
public:
	MS5611_SPI(int bus, spi_dev_e device);
	virtual ~MS5611_SPI();

	virtual int	init();
	virtual int	read(unsigned offset, void *data, unsigned count);
	virtual int	ioctl(unsigned operation, unsigned &arg);

protected:
	virtual int	probe();

private:
	ms5611::prom_u	*_prom

	int		_probe_address(uint8_t address);

	/**
	 * Send a reset command to the MS5611.
	 *
	 * This is required after any bus reset.
	 */
	int		_reset();

	/**
	 * Send a measure command to the MS5611.
	 *
	 * @param addr		Which address to use for the measure operation.
	 */
	int		_measure(unsigned addr);

	/**
	 * Read the MS5611 PROM
	 *
	 * @return		OK if the PROM reads successfully.
	 */
	int		_read_prom();

	/**
	 * Read a 16-bit register value.
	 *
	 * @param reg		The register to read.
	 */
	uint16_t	_reg16(unsigned reg);
};

device::Device *
MS5611_spi_interface()
{
#ifdef PX4_SPIDEV_BARO
	return new MS5611_SPI(1 /* XXX MAGIC NUMBER */, (spi_dev_e)PX4_SPIDEV_BARO);
#endif
	return nullptr;
}

int
MS5611_SPI::init()
{
	int ret;

	ret = SPI::init();
	if (ret != OK)
		goto out;

	/* send reset command */
	ret = _reset();
	if (ret != OK)
		goto out;

	/* read PROM */
	ret = _read_prom();
	if (ret != OK)
		goto out;

out:
	return ret;
}

int
MS5611_SPI::read(unsigned offset, void *data, unsigned count)
{
	union _cvt {
		uint8_t	b[4];
		uint32_t w;
	} *cvt = (_cvt *)data;
	uint8_t buf[4];

	/* read the most recent measurement */
	buf[0] = 0 | DIR_WRITE;
	int ret = transfer(&buf[0], &buf[0], sizeof(buf));

	if (ret == OK) {
		/* fetch the raw value */
		cvt->b[0] = data[3];
		cvt->b[1] = data[2];
		cvt->b[2] = data[1];
		cvt->b[3] = 0;

		ret = count;
	}

	return ret;
}

int
MS5611_SPI::ioctl(unsigned operation, unsigned &arg)
{
	int ret;

	switch (operation) {
	case IOCTL_SET_PROMBUFFER:
		_prom = reinterpret_cast<ms5611_prom_u *>(arg);
		return OK;

	case IOCTL_RESET:
		ret = _reset();
		break;

	case IOCTL_MEASURE:
		ret = _measure(arg);
		break;

	default:
		ret = EINVAL;
	}

	if (ret != OK) {
		errno = ret;
		return -1;
	}
	return 0;
}

int
MS5611_SPI::_reset()
{
	uint8_t cmd = ADDR_RESET_CMD | DIR_WRITE;

	return  transfer(&cmd, nullptr, 1);
}

int
MS5611_SPI::_measure(unsigned addr)
{
	uint8_t cmd = addr | DIR_WRITE;

	return transfer(&cmd, nullptr, 1);
}


int
MS5611_SPI::_read_prom()
{
	/*
	 * Wait for PROM contents to be in the device (2.8 ms) in the case we are
	 * called immediately after reset.
	 */
	usleep(3000);

	/* read and convert PROM words */
	for (int i = 0; i < 8; i++) {
		uint8_t cmd = (ADDR_PROM_SETUP + (i * 2));
		_prom.c[i] = _reg16(cmd);
	}

	/* calculate CRC and return success/failure accordingly */
	return ms5611::crc4(&_prom.c[0]) ? OK : -EIO;
}

uint16_t
MS5611_SPI::_reg16(unsigned reg)
{
	uint8_t cmd[3];

	cmd[0] = reg | DIR_READ;

	transfer(cmd, cmd, sizeof(cmd));

	return (uint16_t)(cmd[1] << 8) | cmd[2];
}
