/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
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
 * @file Driver for the Bosch BMA 180 MEMS accelerometer connected via SPI.
 */

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <systemlib/perf_counter.h>
#include <systemlib/err.h>
#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>

#include <arch/board/up_hrt.h>
#include <arch/board/board.h>

#include <drivers/device/spi.h>
#include <drivers/drv_accel.h>


/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

#define DIR_READ			(1<<7)
#define DIR_WRITE			(0<<7)

#define ADDR_CHIP_ID			0x00
#define CHIP_ID				0x03

#define ADDR_ACC_X_LSB			0x02
#define ADDR_ACC_Y_LSB			0x04
#define ADDR_ACC_Z_LSB			0x06
#define ADDR_TEMPERATURE		0x08

#define ADDR_RESET			0x10
#define SOFT_RESET				0xB6

#define ADDR_BW_TCS			0x20
#define BW_TCS_BW_MASK				(0xf<<4)
#define BW_TCS_BW_10HZ				(0<<4)
#define BW_TCS_BW_20HZ				(1<<4)
#define BW_TCS_BW_40HZ				(2<<4)
#define BW_TCS_BW_75HZ				(3<<4)
#define BW_TCS_BW_150HZ				(4<<4)
#define BW_TCS_BW_300HZ				(5<<4)
#define BW_TCS_BW_600HZ				(6<<4)
#define BW_TCS_BW_1200HZ			(7<<4)

#define ADDR_HIGH_DUR			0x27
#define HIGH_DUR_DIS_I2C			(1<<0)

#define ADDR_TCO_Z			0x30
#define TCO_Z_MODE_MASK				0x3

#define ADDR_GAIN_Y			0x33
#define GAIN_Y_SHADOW_DIS			(1<<0)

#define ADDR_OFFSET_LSB1		0x35
#define OFFSET_LSB1_RANGE_MASK			(7<<1)
#define OFFSET_LSB1_RANGE_1G			(0<<1)
#define OFFSET_LSB1_RANGE_2G			(2<<1)
#define OFFSET_LSB1_RANGE_3G			(3<<1)
#define OFFSET_LSB1_RANGE_4G			(4<<1)
#define OFFSET_LSB1_RANGE_8G			(5<<1)
#define OFFSET_LSB1_RANGE_16G			(6<<1)

#define ADDR_OFFSET_T			0x37
#define OFFSET_T_READOUT_12BIT			(1<<0)

extern "C" { __EXPORT int bma180_main(int argc, char *argv[]); }

class BMA180 : public device::SPI
{
public:
	BMA180(int bus, spi_dev_e device);
	~BMA180();

	virtual int		init();

	virtual ssize_t		read(struct file *filp, char *buffer, size_t buflen);
	virtual int		ioctl(struct file *filp, int cmd, unsigned long arg);

	/**
	 * Diagnostics - print some basic information about the driver.
	 */
	void			print_info();

protected:
	virtual int		probe();

private:

	struct hrt_call		_call;
	unsigned		_call_interval;

	unsigned		_num_reports;
	volatile unsigned	_next_report;
	volatile unsigned	_oldest_report;
	struct accel_report	*_reports;

	struct accel_scale	_accel_scale;
	float			_accel_range_scale;
	float			_accel_range_m_s2;
	orb_advert_t		_accel_topic;

	unsigned		_current_lowpass;
	unsigned		_current_range;

	perf_counter_t		_sample_perf;

	/**
	 * Start automatic measurement.
	 */
	void			start();

	/**
	 * Stop automatic measurement.
	 */
	void			stop();

	/**
	 * Static trampoline from the hrt_call context; because we don't have a
	 * generic hrt wrapper yet.
	 *
	 * Called by the HRT in interrupt context at the specified rate if
	 * automatic polling is enabled.
	 *
	 * @param arg		Instance pointer for the driver that is polling.
	 */
	static void		measure_trampoline(void *arg);

	/**
	 * Fetch measurements from the sensor and update the report ring.
	 */
	void			measure();

	/**
	 * Read a register from the BMA180
	 *
	 * @param		The register to read.
	 * @return		The value that was read.
	 */
	uint8_t			read_reg(unsigned reg);

	/**
	 * Write a register in the BMA180
	 *
	 * @param reg		The register to write.
	 * @param value		The new value to write.
	 */
	void			write_reg(unsigned reg, uint8_t value);

	/**
	 * Modify a register in the BMA180
	 *
	 * Bits are cleared before bits are set.
	 *
	 * @param reg		The register to modify.
	 * @param clearbits	Bits in the register to clear.
	 * @param setbits	Bits in the register to set.
	 */
	void			modify_reg(unsigned reg, uint8_t clearbits, uint8_t setbits);

	/**
	 * Set the BMA180 measurement range.
	 *
	 * @param max_g		The maximum G value the range must support.
	 * @return		OK if the value can be supported, -ERANGE otherwise.
	 */
	int			set_range(unsigned max_g);

	/**
	 * Set the BMA180 internal lowpass filter frequency.
	 *
	 * @param frequency	The internal lowpass filter frequency is set to a value
	 *			equal or greater to this.
	 *			Zero selects the highest frequency supported.
	 * @return		OK if the value can be supported.
	 */
	int			set_lowpass(unsigned frequency);
};

/* helper macro for handling report buffer indices */
#define INCREMENT(_x, _lim)	do { _x++; if (_x >= _lim) _x = 0; } while(0)


BMA180::BMA180(int bus, spi_dev_e device) :
	SPI("BMA180", ACCEL_DEVICE_PATH, bus, device, SPIDEV_MODE3, 8000000),
	_call_interval(0),
	_num_reports(0),
	_next_report(0),
	_oldest_report(0),
	_reports(nullptr),
	_accel_range_scale(0.0f),
	_accel_range_m_s2(0.0f),
	_accel_topic(-1),
	_current_lowpass(0),
	_current_range(0),
	_sample_perf(perf_alloc(PC_ELAPSED, "bma180_read"))
{
	// enable debug() calls
	_debug_enabled = true;

	// default scale factors
	_accel_scale.x_offset = 0;
	_accel_scale.x_scale  = 1.0f;
	_accel_scale.y_offset = 0;
	_accel_scale.y_scale  = 1.0f;
	_accel_scale.z_offset = 0;
	_accel_scale.z_scale  = 1.0f;
}

BMA180::~BMA180()
{
	/* make sure we are truly inactive */
	stop();

	/* free any existing reports */
	if (_reports != nullptr)
		delete[] _reports;

	/* delete the perf counter */
	perf_free(_sample_perf);
}

int
BMA180::init()
{
	int ret = ERROR;

	/* do SPI init (and probe) first */
	if (SPI::init() != OK)
		goto out;

	/* allocate basic report buffers */
	_num_reports = 2;
	_oldest_report = _next_report = 0;
	_reports = new struct accel_report[_num_reports];
	if (_reports == nullptr)
		goto out;

	/* advertise sensor topic */
	memset(&_reports[0], 0, sizeof(_reports[0]));
	_accel_topic = orb_advertise(ORB_ID(sensor_accel), &_reports[0]);

	/* perform soft reset (p48) */
	write_reg(ADDR_RESET, SOFT_RESET);

	/* wait 10us (p49) */
	usleep(10);

	/* disable I2C interface */
	modify_reg(ADDR_HIGH_DUR, HIGH_DUR_DIS_I2C, 0);

	/* switch to low-noise mode */
	modify_reg(ADDR_TCO_Z, TCO_Z_MODE_MASK, 0);

	/* disable 12-bit mode */
	modify_reg(ADDR_OFFSET_T, OFFSET_T_READOUT_12BIT, 0);

	/* disable shadow-disable mode */
	modify_reg(ADDR_GAIN_Y, GAIN_Y_SHADOW_DIS, 0);

	set_range(2);
	set_lowpass(75);

	ret = OK;
out:
	return ret;
}

int
BMA180::probe()
{
	/* dummy read to ensure SPI state machine is sane */
	read_reg(ADDR_CHIP_ID);

	if (read_reg(ADDR_CHIP_ID) == CHIP_ID)
		return OK;

	return -EIO;
}

ssize_t
BMA180::read(struct file *filp, char *buffer, size_t buflen)
{
	unsigned count = buflen / sizeof(struct accel_report);
	int ret = 0;

	/* buffer must be large enough */
	if (count < 1)
		return -ENOSPC;

	/* if automatic measurement is enabled */
	if (_call_interval > 0) {

		/*
		 * While there is space in the caller's buffer, and reports, copy them.
		 * Note that we may be pre-empted by the measurement code while we are doing this;
		 * we are careful to avoid racing with it.
		 */
		while (count--) {
			if (_oldest_report != _next_report) {
				memcpy(buffer, _reports + _oldest_report, sizeof(*_reports));
				ret += sizeof(_reports[0]);
				INCREMENT(_oldest_report, _num_reports);
			}
		}

		/* if there was no data, warn the caller */
		return ret ? ret : -EAGAIN;
	}

	/* manual measurement */
	_oldest_report = _next_report = 0;
	measure();

	/* measurement will have generated a report, copy it out */
	memcpy(buffer, _reports, sizeof(*_reports));
	ret = sizeof(*_reports);

	return ret;
}

int
BMA180::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	switch (cmd) {

	case SENSORIOCSPOLLRATE: {
			switch (arg) {

				/* switching to manual polling */
			case SENSOR_POLLRATE_MANUAL:
				stop();
				_call_interval = 0;
				return OK;

				/* external signalling not supported */
			case SENSOR_POLLRATE_EXTERNAL:

				/* zero would be bad */
			case 0:
				return -EINVAL;


				/* set default/max polling rate */
			case SENSOR_POLLRATE_MAX:
			case SENSOR_POLLRATE_DEFAULT:
				/* XXX 500Hz is just a wild guess */
				return ioctl(filp, SENSORIOCSPOLLRATE, 500);

				/* adjust to a legal polling interval in Hz */
			default: {
					/* do we need to start internal polling? */
					bool want_start = (_call_interval == 0);

					/* convert hz to hrt interval via microseconds */
					unsigned ticks = 1000000 / arg;

					/* check against maximum sane rate */
					if (ticks < 1000)
						return -EINVAL;

					/* update interval for next measurement */
					/* XXX this is a bit shady, but no other way to adjust... */
					_call.period = _call_interval = ticks;

					/* if we need to start the poll state machine, do it */
					if (want_start)
						start();

					return OK;
				}
			}
		}

	case SENSORIOCGPOLLRATE:
		if (_call_interval == 0)
			return SENSOR_POLLRATE_MANUAL;
		return 1000000 / _call_interval;

	case SENSORIOCSQUEUEDEPTH: {
			/* account for sentinel in the ring */
			arg++;

			/* lower bound is mandatory, upper bound is a sanity check */
			if ((arg < 2) || (arg > 100))
				return -EINVAL;

			/* allocate new buffer */
			struct accel_report *buf = new struct accel_report[arg];

			if (nullptr == buf)
				return -ENOMEM;

			/* reset the measurement state machine with the new buffer, free the old */
			stop();
			delete[] _reports;
			_num_reports = arg;
			_reports = buf;
			start();

			return OK;
		}

	case SENSORIOCGQUEUEDEPTH:
		return _num_reports -1;

	case SENSORIOCRESET:
		/* XXX implement */
		return -EINVAL;

	case ACCELIOCSSAMPLERATE:	/* sensor sample rate is not (really) adjustable */
		return -EINVAL;

	case ACCELIOCGSAMPLERATE:
		return 1200;		/* always operating in low-noise mode */

	case ACCELIOCSLOWPASS:
		return set_lowpass(arg);

	case ACCELIOCGLOWPASS:
		return _current_lowpass;

	case ACCELIOCSSCALE:
		/* copy scale in */
		memcpy(&_accel_scale, (struct accel_scale*) arg, sizeof(_accel_scale));
		return OK;

	case ACCELIOCGSCALE:
		/* copy scale out */
		memcpy((struct accel_scale*) arg, &_accel_scale, sizeof(_accel_scale));
		return OK;

	case ACCELIOCSRANGE:
		return set_range(arg);

	case ACCELIOCGRANGE:
		return _current_range;

	default:
		/* give it to the superclass */
		return SPI::ioctl(filp, cmd, arg);
	}
}

uint8_t
BMA180::read_reg(unsigned reg)
{
	uint8_t cmd[2];

	cmd[0] = reg | DIR_READ;

	transfer(cmd, cmd, sizeof(cmd));

	return cmd[1];
}

void
BMA180::write_reg(unsigned reg, uint8_t value)
{
	uint8_t	cmd[2];

	cmd[0] = reg | DIR_WRITE;
	cmd[1] = value;

	transfer(cmd, nullptr, sizeof(cmd));
}

void
BMA180::modify_reg(unsigned reg, uint8_t clearbits, uint8_t setbits)
{
	uint8_t	val;

	val = read_reg(reg);
	val &= ~clearbits;
	val |= setbits;
	write_reg(reg, val);
}

int
BMA180::set_range(unsigned max_g)
{
	uint8_t rangebits;

	if (max_g == 0)
		max_g = 16;
	if (max_g > 16)
		return -ERANGE;

	if (max_g <= 1) {
		_current_range = 1;
		rangebits = OFFSET_LSB1_RANGE_2G;
	} else if (max_g <= 2) {
		_current_range = 2;
		rangebits = OFFSET_LSB1_RANGE_3G;
	} else if (max_g <= 4) {
		_current_range = 4;
		rangebits = OFFSET_LSB1_RANGE_4G;
	} else if (max_g <= 8) {
		_current_range = 8;
		rangebits = OFFSET_LSB1_RANGE_8G;
	} else if (max_g <= 16) {
		_current_range = 16;
		rangebits = OFFSET_LSB1_RANGE_16G;
	} else {
		return -EINVAL;
	}

	/* set new range scaling factor */
	_accel_range_m_s2 = _current_range * 9.80665f;
	_accel_range_scale = _accel_range_m_s2 / 8192.0f;

	/* adjust sensor configuration */
	modify_reg(ADDR_OFFSET_LSB1, OFFSET_LSB1_RANGE_MASK, rangebits);

	return OK;
}

int
BMA180::set_lowpass(unsigned frequency)
{
	uint8_t	bwbits;

	if (frequency > 1200) {
		return -ERANGE;

	} else if (frequency > 600) {
		bwbits = BW_TCS_BW_1200HZ;

	} else if (frequency > 300) {
		bwbits = BW_TCS_BW_600HZ;

	} else if (frequency > 150) {
		bwbits = BW_TCS_BW_300HZ;

	} else if (frequency > 75) {
		bwbits = BW_TCS_BW_150HZ;

	} else if (frequency > 40) {
		bwbits = BW_TCS_BW_75HZ;

	} else if (frequency > 20) {
		bwbits = BW_TCS_BW_40HZ;

	} else if (frequency > 10) {
		bwbits = BW_TCS_BW_20HZ;

	} else {
		bwbits = BW_TCS_BW_10HZ;
	}

	/* adjust sensor configuration */
	modify_reg(ADDR_BW_TCS, BW_TCS_BW_MASK, bwbits);

	return OK;
}

void
BMA180::start()
{
	/* make sure we are stopped first */
	stop();

	/* reset the report ring */
	_oldest_report = _next_report = 0;

	/* start polling at the specified rate */
	hrt_call_every(&_call, 1000, _call_interval, (hrt_callout)&BMA180::measure_trampoline, this);
}

void
BMA180::stop()
{
	hrt_cancel(&_call);
}

void
BMA180::measure_trampoline(void *arg)
{
	BMA180 *dev = (BMA180 *)arg;

	/* make another measurement */
	dev->measure();
}

void
BMA180::measure()
{
	/* BMA180 measurement registers */
#pragma pack(push, 1)
	struct {
		uint8_t		cmd;
		uint16_t	x;
		uint16_t	y;
		uint16_t	z;
	} raw_report;
#pragma pack(pop)

	accel_report		*report = &_reports[_next_report];

	/* start the performance counter */
	perf_begin(_sample_perf);

	/*
	 * Fetch the full set of measurements from the BMA180 in one pass;
	 * starting from the X LSB.
	 */
	raw_report.cmd = ADDR_ACC_X_LSB;
	transfer((uint8_t *)&raw_report, (uint8_t *)&raw_report, sizeof(raw_report));

	/*
	 * Adjust and scale results to SI units.
	 *
	 * Note that we ignore the "new data" bits.  At any time we read, each
	 * of the axis measurements are the "most recent", even if we've seen
	 * them before.  There is no good way to synchronise with the internal
	 * measurement flow without using the external interrupt.
	 */
	_reports[_next_report].timestamp = hrt_absolute_time();
	/* XXX adjust for sensor alignment to board here */
	report->x_raw = raw_report.x;
	report->y_raw = raw_report.y;
	report->z_raw = raw_report.z;

	report->x = ((report->x_raw * _accel_range_scale) - _accel_scale.x_offset) * _accel_scale.x_scale;
	report->y = ((report->y_raw * _accel_range_scale) - _accel_scale.y_offset) * _accel_scale.y_scale;
	report->z = ((report->z_raw * _accel_range_scale) - _accel_scale.z_offset) * _accel_scale.z_scale;
	report->scaling = _accel_range_scale;
	report->range_m_s2 = _accel_range_m_s2;

	/* post a report to the ring - note, not locked */
	INCREMENT(_next_report, _num_reports);

	/* if we are running up against the oldest report, fix it */
	if (_next_report == _oldest_report)
		INCREMENT(_oldest_report, _num_reports);

	/* notify anyone waiting for data */
	poll_notify(POLLIN);

	/* publish for subscribers */
	orb_publish(ORB_ID(sensor_accel), _accel_topic, report);

	/* stop the perf counter */
	perf_end(_sample_perf);
}

void
BMA180::print_info()
{
	perf_print_counter(_sample_perf);
	printf("report queue:   %u (%u/%u @ %p)\n",
	       _num_reports, _oldest_report, _next_report, _reports);
}

/**
 * Local functions in support of the shell command.
 */
namespace bma180
{

BMA180	*g_dev;

void	start();
void	test();
void	reset();
void	info();

/**
 * Start the driver.
 */
void
start()
{
	int fd;

	if (g_dev != nullptr)
		errx(1, "already started");

	/* create the driver */
	g_dev = new BMA180(1 /* XXX magic number */, (spi_dev_e)PX4_SPIDEV_ACCEL);

	if (g_dev == nullptr)
		goto fail;

	if (OK != g_dev->init())
		goto fail;

	/* set the poll rate to default, starts automatic data collection */
	fd = open(ACCEL_DEVICE_PATH, O_RDONLY);
	if (fd < 0)
		goto fail;
	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0)
		goto fail;

	exit(0);
fail:
	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;
	}
	errx(1, "driver start failed");
}

/**
 * Perform some basic functional tests on the driver;
 * make sure we can collect data from the sensor in polled
 * and automatic modes.
 */
void
test()
{
	int fd = -1;
	struct accel_report a_report;
	ssize_t sz;

	/* get the driver */
	fd = open(ACCEL_DEVICE_PATH, O_RDONLY);
	if (fd < 0)
		err(1, "%s open failed (try 'bma180 start' if the driver is not running)", 
			ACCEL_DEVICE_PATH);

	/* reset to manual polling */
	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_MANUAL) < 0)
		err(1, "reset to manual polling");
	
	/* do a simple demand read */
	sz = read(fd, &a_report, sizeof(a_report));
	if (sz != sizeof(a_report))
		err(1, "immediate acc read failed");

	warnx("single read");
	warnx("time:     %lld", a_report.timestamp);
	warnx("acc  x:  \t%8.4f\tm/s^2", (double)a_report.x);
	warnx("acc  y:  \t%8.4f\tm/s^2", (double)a_report.y);
	warnx("acc  z:  \t%8.4f\tm/s^2", (double)a_report.z);
	warnx("acc  x:  \t%d\traw 0x%0x", (short)a_report.x_raw, (unsigned short)a_report.x_raw);
	warnx("acc  y:  \t%d\traw 0x%0x", (short)a_report.y_raw, (unsigned short)a_report.y_raw);
	warnx("acc  z:  \t%d\traw 0x%0x", (short)a_report.z_raw, (unsigned short)a_report.z_raw);
	warnx("acc range: %8.4f m/s^2 (%8.4f g)", (double)a_report.range_m_s2,
		(double)(a_report.range_m_s2 / 9.81f));

	/* XXX add poll-rate tests here too */

	reset();
	errx(0, "PASS");
}

/**
 * Reset the driver.
 */
void
reset()
{
	int fd = open(ACCEL_DEVICE_PATH, O_RDONLY);
	if (fd < 0)
		err(1, "failed ");
	if (ioctl(fd, SENSORIOCRESET, 0) < 0)
		err(1, "driver reset failed");
	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0)
		err(1, "driver poll restart failed");

	exit(0);
}

/**
 * Print a little info about the driver.
 */
void
info()
{
	if (g_dev == nullptr)
		errx(1, "BMA180: driver not running");

	printf("state @ %p\n", g_dev);
	g_dev->print_info();

	exit(0);
}


} // namespace

int
bma180_main(int argc, char *argv[])
{
	/*
	 * Start/load the driver.

	 */
	if (!strcmp(argv[1], "start"))
		bma180::start();

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(argv[1], "test"))
		bma180::test();

	/*
	 * Reset the driver.
	 */
	if (!strcmp(argv[1], "reset"))
		bma180::reset();

	/*
	 * Print driver information.
	 */
	if (!strcmp(argv[1], "info"))
		bma180::info();

	errx(1, "unrecognised command, try 'start', 'test', 'reset' or 'info'");
}
