/****************************************************************************
 *
 *   Copyright (c) 2014 PX4 Development Team. All rights reserved.
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
 * @file sf0x.cpp
 * @author Lorenz Meier <lm@inf.ethz.ch>
 * @author Greg Hulands
 *
 * Driver for the Lightware SF0x laser rangefinder series
 */

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>

#include <systemlib/perf_counter.h>
#include <systemlib/err.h>

#include <drivers/drv_hrt.h>
#include <drivers/drv_range_finder.h>
#include <drivers/device/device.h>
#include <drivers/device/ringbuffer.h>

#include <uORB/uORB.h>
#include <uORB/topics/subsystem_info.h>

#include <board_config.h>

/* Configuration Constants */

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif

#define SF0X_CONVERSION_INTERVAL	84000
#define SF0X_TAKE_RANGE_REG		'D'
#define SF02F_MIN_DISTANCE		0.0f
#define SF02F_MAX_DISTANCE		40.0f

class SF0X : public device::CDev
{
public:
	SF0X(const char* port="/dev/ttyS1");
	virtual ~SF0X();

	virtual int 			init();

	virtual ssize_t			read(struct file *filp, char *buffer, size_t buflen);
	virtual int			ioctl(struct file *filp, int cmd, unsigned long arg);

	/**
	* Diagnostics - print some basic information about the driver.
	*/
	void				print_info();

protected:
	virtual int			probe();

private:
	float				_min_distance;
	float				_max_distance;
	work_s				_work;
	RingBuffer			*_reports;
	bool				_sensor_ok;
	int				_measure_ticks;
	bool				_collect_phase;
	int				_fd;

	orb_advert_t			_range_finder_topic;

	perf_counter_t			_sample_perf;
	perf_counter_t			_comms_errors;
	perf_counter_t			_buffer_overflows;

	/**
	* Initialise the automatic measurement state machine and start it.
	*
	* @note This function is called at open and error time.  It might make sense
	*       to make it more aggressive about resetting the bus in case of errors.
	*/
	void				start();

	/**
	* Stop the automatic measurement state machine.
	*/
	void				stop();

	/**
	* Set the min and max distance thresholds if you want the end points of the sensors
	* range to be brought in at all, otherwise it will use the defaults SF0X_MIN_DISTANCE
	* and SF0X_MAX_DISTANCE
	*/
	void				set_minimum_distance(float min);
	void				set_maximum_distance(float max);
	float				get_minimum_distance();
	float				get_maximum_distance();

	/**
	* Perform a poll cycle; collect from the previous measurement
	* and start a new one.
	*/
	void				cycle();
	int				measure();
	int				collect();
	/**
	* Static trampoline from the workq context; because we don't have a
	* generic workq wrapper yet.
	*
	* @param arg		Instance pointer for the driver that is polling.
	*/
	static void			cycle_trampoline(void *arg);


};

/*
 * Driver 'main' command.
 */
extern "C" __EXPORT int sf0x_main(int argc, char *argv[]);

SF0X::SF0X(const char *port) :
	CDev("SF0X", RANGE_FINDER_DEVICE_PATH),
	_min_distance(SF02F_MIN_DISTANCE),
	_max_distance(SF02F_MAX_DISTANCE),
	_reports(nullptr),
	_sensor_ok(false),
	_measure_ticks(0),
	_collect_phase(false),
	_range_finder_topic(-1),
	_sample_perf(perf_alloc(PC_ELAPSED, "sf0x_read")),
	_comms_errors(perf_alloc(PC_COUNT, "sf0x_comms_errors")),
	_buffer_overflows(perf_alloc(PC_COUNT, "sf0x_buffer_overflows"))
{
	/* open fd */
	_fd = ::open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);

	// enable debug() calls
	_debug_enabled = true;

	// work_cancel in the dtor will explode if we don't do this...
	memset(&_work, 0, sizeof(_work));
}

SF0X::~SF0X()
{
	/* make sure we are truly inactive */
	stop();

	/* free any existing reports */
	if (_reports != nullptr) {
		delete _reports;
	}
}

int
SF0X::init()
{
	int ret = ERROR;

	/* allocate basic report buffers */
	_reports = new RingBuffer(2, sizeof(range_finder_report));

	if (_reports == nullptr) {
		goto out;
	}

	/* get a publish handle on the range finder topic */
	struct range_finder_report zero_report;
	memset(&zero_report, 0, sizeof(zero_report));
	_range_finder_topic = orb_advertise(ORB_ID(sensor_range_finder), &zero_report);

	if (_range_finder_topic < 0) {
		debug("failed to create sensor_range_finder object. Did you start uOrb?");
	}

	ret = OK;
	/* sensor is ok, but we don't really know if it is within range */
	_sensor_ok = true;
out:
	return ret;
}

int
SF0X::probe()
{
	return measure();
}

void
SF0X::set_minimum_distance(float min)
{
	_min_distance = min;
}

void
SF0X::set_maximum_distance(float max)
{
	_max_distance = max;
}

float
SF0X::get_minimum_distance()
{
	return _min_distance;
}

float
SF0X::get_maximum_distance()
{
	return _max_distance;
}

int
SF0X::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	switch (cmd) {

	case SENSORIOCSPOLLRATE: {
			switch (arg) {

			/* switching to manual polling */
			case SENSOR_POLLRATE_MANUAL:
				stop();
				_measure_ticks = 0;
				return OK;

			/* external signalling (DRDY) not supported */
			case SENSOR_POLLRATE_EXTERNAL:

			/* zero would be bad */
			case 0:
				return -EINVAL;

			/* set default/max polling rate */
			case SENSOR_POLLRATE_MAX:
			case SENSOR_POLLRATE_DEFAULT: {
					/* do we need to start internal polling? */
					bool want_start = (_measure_ticks == 0);

					/* set interval for next measurement to minimum legal value */
					_measure_ticks = USEC2TICK(SF0X_CONVERSION_INTERVAL);

					/* if we need to start the poll state machine, do it */
					if (want_start) {
						start();
					}

					return OK;
				}

			/* adjust to a legal polling interval in Hz */
			default: {
					/* do we need to start internal polling? */
					bool want_start = (_measure_ticks == 0);

					/* convert hz to tick interval via microseconds */
					unsigned ticks = USEC2TICK(1000000 / arg);

					/* check against maximum rate */
					if (ticks < USEC2TICK(SF0X_CONVERSION_INTERVAL)) {
						return -EINVAL;
					}

					/* update interval for next measurement */
					_measure_ticks = ticks;

					/* if we need to start the poll state machine, do it */
					if (want_start) {
						start();
					}

					return OK;
				}
			}
		}

	case SENSORIOCGPOLLRATE:
		if (_measure_ticks == 0) {
			return SENSOR_POLLRATE_MANUAL;
		}

		return (1000 / _measure_ticks);

	case SENSORIOCSQUEUEDEPTH: {
			/* lower bound is mandatory, upper bound is a sanity check */
			if ((arg < 1) || (arg > 100)) {
				return -EINVAL;
			}

			irqstate_t flags = irqsave();

			if (!_reports->resize(arg)) {
				irqrestore(flags);
				return -ENOMEM;
			}

			irqrestore(flags);

			return OK;
		}

	case SENSORIOCGQUEUEDEPTH:
		return _reports->size();

	case SENSORIOCRESET:
		/* XXX implement this */
		return -EINVAL;

	case RANGEFINDERIOCSETMINIUMDISTANCE: {
			set_minimum_distance(*(float *)arg);
			return 0;
		}
		break;

	case RANGEFINDERIOCSETMAXIUMDISTANCE: {
			set_maximum_distance(*(float *)arg);
			return 0;
		}
		break;

	default:
		/* give it to the superclass */
		return CDev::ioctl(filp, cmd, arg);
	}
}

ssize_t
SF0X::read(struct file *filp, char *buffer, size_t buflen)
{
	unsigned count = buflen / sizeof(struct range_finder_report);
	struct range_finder_report *rbuf = reinterpret_cast<struct range_finder_report *>(buffer);
	int ret = 0;

	/* buffer must be large enough */
	if (count < 1) {
		return -ENOSPC;
	}

	/* if automatic measurement is enabled */
	if (_measure_ticks > 0) {

		/*
		 * While there is space in the caller's buffer, and reports, copy them.
		 * Note that we may be pre-empted by the workq thread while we are doing this;
		 * we are careful to avoid racing with them.
		 */
		while (count--) {
			if (_reports->get(rbuf)) {
				ret += sizeof(*rbuf);
				rbuf++;
			}
		}

		/* if there was no data, warn the caller */
		return ret ? ret : -EAGAIN;
	}

	/* manual measurement - run one conversion */
	do {
		_reports->flush();

		/* trigger a measurement */
		if (OK != measure()) {
			ret = -EIO;
			break;
		}

		/* wait for it to complete */
		usleep(SF0X_CONVERSION_INTERVAL);

		/* run the collection phase */
		if (OK != collect()) {
			ret = -EIO;
			break;
		}

		/* state machine will have generated a report, copy it out */
		if (_reports->get(rbuf)) {
			ret = sizeof(*rbuf);
		}

	} while (0);

	return ret;
}

int
SF0X::measure()
{
	int ret;

	/*
	 * Send the command to begin a measurement.
	 */
	char cmd = SF0X_TAKE_RANGE_REG;
	ret = ::write(_fd, &cmd, 1);

	if (OK != sizeof(cmd)) {
		perf_count(_comms_errors);
		log("serial transfer returned %d", ret);
		return ret;
	}

	ret = OK;

	return ret;
}

int
SF0X::collect()
{
	int	ret;

	perf_begin(_sample_perf);

	char buf[16];
	/* read from the sensor (uart buffer) */
	ret = ::read(_fd, buf, sizeof(buf));

	if (ret < 1) {
		log("error reading from sensor: %d", ret);
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return ret;
	}

	printf("ret: %d, val (str): %s\n", ret, buf);
	char* end;
	float si_units = strtod(buf,&end);
	printf("val (float): %8.4f\n", si_units);

	struct range_finder_report report;

	/* this should be fairly close to the end of the measurement, so the best approximation of the time */
	report.timestamp = hrt_absolute_time();
	report.error_count = perf_event_count(_comms_errors);
	report.distance = si_units;
	report.valid = si_units > get_minimum_distance() && si_units < get_maximum_distance() ? 1 : 0;

	/* publish it */
	orb_publish(ORB_ID(sensor_range_finder), _range_finder_topic, &report);

	if (_reports->force(&report)) {
		perf_count(_buffer_overflows);
	}

	/* notify anyone waiting for data */
	poll_notify(POLLIN);

	ret = OK;

	perf_end(_sample_perf);
	return ret;
}

void
SF0X::start()
{
	/* reset the report ring and state machine */
	_collect_phase = false;
	_reports->flush();

	/* schedule a cycle to start things */
	work_queue(HPWORK, &_work, (worker_t)&SF0X::cycle_trampoline, this, 1);

	/* notify about state change */
	struct subsystem_info_s info = {
		true,
		true,
		true,
		SUBSYSTEM_TYPE_RANGEFINDER
	};
	static orb_advert_t pub = -1;

	if (pub > 0) {
		orb_publish(ORB_ID(subsystem_info), pub, &info);

	} else {
		pub = orb_advertise(ORB_ID(subsystem_info), &info);
	}
}

void
SF0X::stop()
{
	work_cancel(HPWORK, &_work);
}

void
SF0X::cycle_trampoline(void *arg)
{
	SF0X *dev = static_cast<SF0X *>(arg);

	dev->cycle();
}

void
SF0X::cycle()
{
	/* collection phase? */
	if (_collect_phase) {

		/* perform collection */
		if (OK != collect()) {
			log("collection error");
			/* restart the measurement state machine */
			start();
			return;
		}

		/* next phase is measurement */
		_collect_phase = false;

		/*
		 * Is there a collect->measure gap?
		 */
		if (_measure_ticks > USEC2TICK(SF0X_CONVERSION_INTERVAL)) {

			/* schedule a fresh cycle call when we are ready to measure again */
			work_queue(HPWORK,
				   &_work,
				   (worker_t)&SF0X::cycle_trampoline,
				   this,
				   _measure_ticks - USEC2TICK(SF0X_CONVERSION_INTERVAL));

			return;
		}
	}

	/* measurement phase */
	if (OK != measure()) {
		log("measure error");
	}

	/* next phase is collection */
	_collect_phase = true;

	/* schedule a fresh cycle call when the measurement is done */
	work_queue(HPWORK,
		   &_work,
		   (worker_t)&SF0X::cycle_trampoline,
		   this,
		   USEC2TICK(SF0X_CONVERSION_INTERVAL));
}

void
SF0X::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_buffer_overflows);
	printf("poll interval:  %d ticks\n", _measure_ticks);
	_reports->print_info("report queue");
}

/**
 * Local functions in support of the shell command.
 */
namespace sf0x
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
const int ERROR = -1;

SF0X	*g_dev;

void	start();
void	stop();
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

	if (g_dev != nullptr) {
		errx(1, "already started");
	}

	/* create the driver */
	g_dev = new SF0X();

	if (g_dev == nullptr) {
		goto fail;
	}

	if (OK != g_dev->init()) {
		goto fail;
	}

	/* set the poll rate to default, starts automatic data collection */
	fd = open(RANGE_FINDER_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		goto fail;
	}

	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0) {
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
 * Stop the driver
 */
void stop()
{
	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;

	} else {
		errx(1, "driver not running");
	}

	exit(0);
}

/**
 * Perform some basic functional tests on the driver;
 * make sure we can collect data from the sensor in polled
 * and automatic modes.
 */
void
test()
{
	struct range_finder_report report;
	ssize_t sz;

	int fd = open(RANGE_FINDER_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		err(1, "%s open failed (try 'sf0x start' if the driver is not running", RANGE_FINDER_DEVICE_PATH);
	}

	/* do a simple demand read */
	sz = read(fd, &report, sizeof(report));

	if (sz != sizeof(report)) {
		err(1, "immediate read failed");
	}

	warnx("single read");
	warnx("measurement: %0.2f m", (double)report.distance);
	warnx("time:        %lld", report.timestamp);

	/* start the sensor polling at 2Hz */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 2)) {
		errx(1, "failed to set 2Hz poll rate");
	}

	/* read the sensor 5x and report each value */
	for (unsigned i = 0; i < 5; i++) {
		struct pollfd fds;

		/* wait for data to be ready */
		fds.fd = fd;
		fds.events = POLLIN;
		int ret = poll(&fds, 1, 2000);

		if (ret != 1) {
			errx(1, "timed out waiting for sensor data");
		}

		/* now go get it */
		sz = read(fd, &report, sizeof(report));

		if (sz != sizeof(report)) {
			err(1, "periodic read failed");
		}

		warnx("periodic read %u", i);
		warnx("measurement: %0.3f", (double)report.distance);
		warnx("time:        %lld", report.timestamp);
	}

	errx(0, "PASS");
}

/**
 * Reset the driver.
 */
void
reset()
{
	int fd = open(RANGE_FINDER_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		err(1, "failed ");
	}

	if (ioctl(fd, SENSORIOCRESET, 0) < 0) {
		err(1, "driver reset failed");
	}

	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0) {
		err(1, "driver poll restart failed");
	}

	exit(0);
}

/**
 * Print a little info about the driver.
 */
void
info()
{
	if (g_dev == nullptr) {
		errx(1, "driver not running");
	}

	printf("state @ %p\n", g_dev);
	g_dev->print_info();

	exit(0);
}

} // namespace

int
sf0x_main(int argc, char *argv[])
{
	/*
	 * Start/load the driver.
	 */
	if (!strcmp(argv[1], "start")) {
		sf0x::start();
	}

	/*
	 * Stop the driver
	 */
	if (!strcmp(argv[1], "stop")) {
		sf0x::stop();
	}

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(argv[1], "test")) {
		sf0x::test();
	}

	/*
	 * Reset the driver.
	 */
	if (!strcmp(argv[1], "reset")) {
		sf0x::reset();
	}

	/*
	 * Print driver information.
	 */
	if (!strcmp(argv[1], "info") || !strcmp(argv[1], "status")) {
		sf0x::info();
	}

	errx(1, "unrecognized command, try 'start', 'test', 'reset' or 'info'");
}
