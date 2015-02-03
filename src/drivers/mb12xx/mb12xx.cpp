/****************************************************************************
 *
 *   Copyright (c) 2013-2015 PX4 Development Team. All rights reserved.
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
 * @file mb12xx.cpp
 * @author Greg Hulands
 * @author Jon Verbeke <jon.verbeke@kuleuven.be>
 *
 * Driver for the Maxbotix sonar range finders connected via I2C.
 */

#include <nuttx/config.h>

#include <drivers/device/i2c.h>

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
#include <vector>

#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>

#include <systemlib/perf_counter.h>
#include <systemlib/err.h>

#include <drivers/drv_hrt.h>
#include <drivers/drv_range_finder.h>
#include <drivers/device/ringbuffer.h>

#include <uORB/uORB.h>
#include <uORB/topics/subsystem_info.h>

#include <board_config.h>

/* Configuration Constants */
#define MB12XX_BUS 		PX4_I2C_BUS_EXPANSION
#define MB12XX_BASEADDR 	0x70 /* 7-bit address. 8-bit address is 0xE0 */
#define MB12XX_DEVICE_PATH	"/dev/mb12xx"

/* MB12xx Registers addresses */

#define MB12XX_TAKE_RANGE_REG	0x51		/* Measure range Register */
#define MB12XX_SET_ADDRESS_1	0xAA		/* Change address 1 Register */
#define MB12XX_SET_ADDRESS_2	0xA5		/* Change address 2 Register */

/* Device limits */
#define MB12XX_MIN_DISTANCE 	(0.20f)
#define MB12XX_MAX_DISTANCE 	(7.65f)

#define MB12XX_CONVERSION_INTERVAL 	100000 /* 60ms for one sonar */
#define TICKS_BETWEEN_SUCCESIVE_FIRES 	100000 /* 30ms between each sonar measurement (watch out for interference!) */


/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif

class MB12XX : public device::I2C
{
public:
	MB12XX(int bus = MB12XX_BUS, int address = MB12XX_BASEADDR);
	virtual ~MB12XX();

	virtual int 		init();

	virtual ssize_t		read(struct file *filp, char *buffer, size_t buflen);
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
	RingBuffer		*_reports;
	bool				_sensor_ok;
	int					_measure_ticks;
	bool				_collect_phase;
	int					_class_instance;

	orb_advert_t		_range_finder_topic;

	perf_counter_t		_sample_perf;
	perf_counter_t		_comms_errors;
	perf_counter_t		_buffer_overflows;

	uint8_t				_cycle_counter;	/* counter in cycle to change i2c adresses */
	int					_cycling_rate;	/* */
	uint8_t				_index_counter;	/* temporary sonar i2c address */
	std::vector<uint8_t>	addr_ind; 	/* temp sonar i2c address vector */
	std::vector<float>	_latest_sonar_measurements; /* vector to store latest sonar measurements in before writing to report */


	/**
	* Test whether the device supported by the driver is present at a
	* specific address.
	*
	* @param address	The I2C bus address to probe.
	* @return			True if the device is present.
	*/
	int					probe_address(uint8_t address);

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
	* range to be brought in at all, otherwise it will use the defaults MB12XX_MIN_DISTANCE
	* and MB12XX_MAX_DISTANCE
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
	int					measure();
	int					collect();
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
extern "C" __EXPORT int mb12xx_main(int argc, char *argv[]);

MB12XX::MB12XX(int bus, int address) :
	I2C("MB12xx", MB12XX_DEVICE_PATH, bus, address, 100000),
	_min_distance(MB12XX_MIN_DISTANCE),
	_max_distance(MB12XX_MAX_DISTANCE),
	_reports(nullptr),
	_sensor_ok(false),
	_measure_ticks(0),
	_collect_phase(false),
	_class_instance(-1),
	_range_finder_topic(-1),
	_sample_perf(perf_alloc(PC_ELAPSED, "mb12xx_read")),
	_comms_errors(perf_alloc(PC_COUNT, "mb12xx_comms_errors")),
	_buffer_overflows(perf_alloc(PC_COUNT, "mb12xx_buffer_overflows")),
	_cycle_counter(0),	/* initialising counter for cycling function to zero */
	_cycling_rate(0),	/* initialising cycling rate (which can differ depending on one sonar or multiple) */
	_index_counter(0) 	/* initialising temp sonar i2c address to zero */

{
	/* enable debug() calls */
	_debug_enabled = false;

	/* work_cancel in the dtor will explode if we don't do this... */
	memset(&_work, 0, sizeof(_work));
}

MB12XX::~MB12XX()
{
	/* make sure we are truly inactive */
	stop();

	/* free any existing reports */
	if (_reports != nullptr) {
		delete _reports;
	}

	if (_class_instance != -1) {
		unregister_class_devname(RANGE_FINDER_BASE_DEVICE_PATH, _class_instance);
	}

	/* free perf counters */
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_buffer_overflows);
}

int
MB12XX::init()
{
	int ret = ERROR;

	/* do I2C init (and probe) first */
	if (I2C::init() != OK) {
		goto out;
	}

	/* allocate basic report buffers */
	_reports = new RingBuffer(2, sizeof(range_finder_report));

	_index_counter = MB12XX_BASEADDR;	/* set temp sonar i2c address to base adress */
	set_address(_index_counter);		/* set I2c port to temp sonar i2c adress */

	if (_reports == nullptr) {
		goto out;
	}

	_class_instance = register_class_devname(RANGE_FINDER_BASE_DEVICE_PATH);

	if (_class_instance == CLASS_DEVICE_PRIMARY) {
		/* get a publish handle on the range finder topic */
		struct range_finder_report rf_report = {};

		_range_finder_topic = orb_advertise(ORB_ID(sensor_range_finder), &rf_report);

		if (_range_finder_topic < 0) {
			log("failed to create sensor_range_finder object. Did you start uOrb?");
		}
	}

	// XXX we should find out why we need to wait 200 ms here
	usleep(200000);

	/* check for connected rangefinders on each i2c port:
	   We start from i2c base address (0x70 = 112) and count downwards
	   So second iteration it uses i2c address 111, third iteration 110 and so on*/
	for (unsigned counter = 0; counter <= MB12XX_MAX_RANGEFINDERS; counter++) {
		_index_counter = MB12XX_BASEADDR - counter;	/* set temp sonar i2c address to base adress - counter */
		set_address(_index_counter);			/* set I2c port to temp sonar i2c adress */
		int ret2 = measure();

		if (ret2 == 0) { /* sonar is present -> store address_index in array */
			addr_ind.push_back(_index_counter);
			debug("sonar added");
			_latest_sonar_measurements.push_back(200);
		}
	}

	_index_counter = MB12XX_BASEADDR;
	set_address(_index_counter); /* set i2c port back to base adress for rest of driver */

	/* if only one sonar detected, no special timing is required between firing, so use default */
	if (addr_ind.size() == 1) {
		_cycling_rate = MB12XX_CONVERSION_INTERVAL;

	} else {
		_cycling_rate = TICKS_BETWEEN_SUCCESIVE_FIRES;
	}

	/* show the connected sonars in terminal */
	for (unsigned i = 0; i < addr_ind.size(); i++) {
		log("sonar %d with address %d added", (i + 1), addr_ind[i]);
	}

	debug("Number of sonars connected: %d", addr_ind.size());

	ret = OK;
	/* sensor is ok, but we don't really know if it is within range */
	_sensor_ok = true;
out:
	return ret;
}

int
MB12XX::probe()
{
	return measure();
}

void
MB12XX::set_minimum_distance(float min)
{
	_min_distance = min;
}

void
MB12XX::set_maximum_distance(float max)
{
	_max_distance = max;
}

float
MB12XX::get_minimum_distance()
{
	return _min_distance;
}

float
MB12XX::get_maximum_distance()
{
	return _max_distance;
}

int
MB12XX::ioctl(struct file *filp, int cmd, unsigned long arg)
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
					_measure_ticks = USEC2TICK(_cycling_rate);

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
					int ticks = USEC2TICK(1000000 / arg);

					/* check against maximum rate */
					if (ticks < USEC2TICK(_cycling_rate)) {
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
		return I2C::ioctl(filp, cmd, arg);
	}
}

ssize_t
MB12XX::read(struct file *filp, char *buffer, size_t buflen)
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
		usleep(_cycling_rate * 2);

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
MB12XX::measure()
{

	int ret;

	/*
	 * Send the command to begin a measurement.
	 */

	uint8_t cmd = MB12XX_TAKE_RANGE_REG;
	ret = transfer(&cmd, 1, nullptr, 0);

	if (OK != ret) {
		perf_count(_comms_errors);
		debug("i2c::transfer returned %d", ret);
		return ret;
	}

	ret = OK;

	return ret;
}

int
MB12XX::collect()
{
	int	ret = -EIO;

	/* read from the sensor */
	uint8_t val[2] = {0, 0};

	perf_begin(_sample_perf);

	ret = transfer(nullptr, 0, &val[0], 2);

	if (ret < 0) {
		debug("error reading from sensor: %d", ret);
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return ret;
	}

	uint16_t distance = val[0] << 8 | val[1];
	float si_units = (distance * 1.0f) / 100.0f; /* cm to m */
	struct range_finder_report report;

	/* this should be fairly close to the end of the measurement, so the best approximation of the time */
	report.timestamp = hrt_absolute_time();
	report.error_count = perf_event_count(_comms_errors);

	/* if only one sonar, write it to the original distance parameter so that it's still used as altitude sonar */
	if (addr_ind.size() == 1) {
		report.distance = si_units;

		for (unsigned i = 0; i < (MB12XX_MAX_RANGEFINDERS); i++) {
			report.distance_vector[i] = 0;
		}

		report.just_updated = 0;

	} else {
		/* for multiple sonars connected */

		/* don't use the orginial single sonar variable */
		report.distance = 0;

		/* intermediate vector _latest_sonar_measurements is used to store the measurements as every cycle the other sonar values 			of the report are thrown away and/or filled in with garbage. We don't want this. We want the report to give the latest 			value for each connected sonar */
		_latest_sonar_measurements[_cycle_counter] = si_units;

		for (unsigned i = 0; i < (_latest_sonar_measurements.size()); i++) {
			report.distance_vector[i] = _latest_sonar_measurements[i];
		}

		/* a just_updated variable is added to indicate to autopilot (ardupilot or whatever) which sonar has most recently been 		collected as this could be of use for Kalman filters */
		report.just_updated = _index_counter;

		/* Make sure all elements of the distance vector for which no sonar is connected are zero to prevent strange numbers */
		for (unsigned i = 0; i < (MB12XX_MAX_RANGEFINDERS - addr_ind.size()); i++) {
			report.distance_vector[addr_ind.size() + i] = 0;
		}
	}

	report.minimum_distance = get_minimum_distance();
	report.maximum_distance = get_maximum_distance();
	report.valid = si_units > get_minimum_distance() && si_units < get_maximum_distance() ? 1 : 0;

	/* publish it, if we are the primary */
	if (_range_finder_topic >= 0) {
		orb_publish(ORB_ID(sensor_range_finder), _range_finder_topic, &report);
	}

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
MB12XX::start()
{

	/* reset the report ring and state machine */
	_collect_phase = false;
	_reports->flush();

	/* schedule a cycle to start things */
	work_queue(HPWORK, &_work, (worker_t)&MB12XX::cycle_trampoline, this, 5);

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
MB12XX::stop()
{
	work_cancel(HPWORK, &_work);
}

void
MB12XX::cycle_trampoline(void *arg)
{

	MB12XX *dev = (MB12XX *)arg;

	dev->cycle();

}

void
MB12XX::cycle()
{
	if (_collect_phase) {
		_index_counter = addr_ind[_cycle_counter]; /*sonar from previous iteration collect is now read out */
		set_address(_index_counter);

		/* perform collection */
		if (OK != collect()) {
			debug("collection error");
			/* if error restart the measurement state machine */
			start();
			return;
		}

		/* next phase is measurement */
		_collect_phase = false;

		/* change i2c adress to next sonar */
		_cycle_counter = _cycle_counter + 1;

		if (_cycle_counter >= addr_ind.size()) {
			_cycle_counter = 0;
		}

		/* Is there a collect->measure gap? Yes, and the timing is set equal to the cycling_rate
		   Otherwise the next sonar would fire without the first one having received its reflected sonar pulse */

		if (_measure_ticks > USEC2TICK(_cycling_rate)) {

			/* schedule a fresh cycle call when we are ready to measure again */
			work_queue(HPWORK,
				   &_work,
				   (worker_t)&MB12XX::cycle_trampoline,
				   this,
				   _measure_ticks - USEC2TICK(_cycling_rate));
			return;
		}
	}

	/* Measurement (firing) phase */

	/* ensure sonar i2c adress is still correct */
	_index_counter = addr_ind[_cycle_counter];
	set_address(_index_counter);

	/* Perform measurement */
	if (OK != measure()) {
		debug("measure error sonar adress %d", _index_counter);
	}

	/* next phase is collection */
	_collect_phase = true;

	/* schedule a fresh cycle call when the measurement is done */
	work_queue(HPWORK,
		   &_work,
		   (worker_t)&MB12XX::cycle_trampoline,
		   this,
		   USEC2TICK(_cycling_rate));

}

void
MB12XX::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_buffer_overflows);
	printf("poll interval:  %u ticks\n", _measure_ticks);
	_reports->print_info("report queue");
}

/**
 * Local functions in support of the shell command.
 */
namespace mb12xx
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
const int ERROR = -1;

MB12XX	*g_dev;

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
	g_dev = new MB12XX(MB12XX_BUS);

	if (g_dev == nullptr) {
		goto fail;
	}

	if (OK != g_dev->init()) {
		goto fail;
	}

	/* set the poll rate to default, starts automatic data collection */
	fd = open(MB12XX_DEVICE_PATH, O_RDONLY);

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
	int ret;

	int fd = open(MB12XX_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		err(1, "%s open failed (try 'mb12xx start' if the driver is not running", MB12XX_DEVICE_PATH);
	}

	/* do a simple demand read */
	sz = read(fd, &report, sizeof(report));

	if (sz != sizeof(report)) {
		err(1, "immediate read failed");
	}

	warnx("single read");
	warnx("measurement: %0.2f of sonar %d", (double)report.distance_vector[report.just_updated], report.just_updated);
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
		ret = poll(&fds, 1, 2000);

		if (ret != 1) {
			errx(1, "timed out waiting for sensor data");
		}

		/* now go get it */
		sz = read(fd, &report, sizeof(report));

		if (sz != sizeof(report)) {
			err(1, "periodic read failed");
		}

		warnx("periodic read %u", i);

		/* Print the sonar rangefinder report sonar distance vector */
		for (uint8_t count = 0; count < MB12XX_MAX_RANGEFINDERS; count++) {
			warnx("measurement: %0.3f of sonar %u", (double)report.distance_vector[count], count + 1);
		}

		warnx("time:        %lld", report.timestamp);
	}

	/* reset the sensor polling to default rate */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT)) {
		errx(1, "failed to set default poll rate");
	}

	errx(0, "PASS");
}

/**
 * Reset the driver.
 */
void
reset()
{
	int fd = open(MB12XX_DEVICE_PATH, O_RDONLY);

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

} /* namespace */

int
mb12xx_main(int argc, char *argv[])
{
	/*
	 * Start/load the driver.
	 */
	if (!strcmp(argv[1], "start")) {
		mb12xx::start();
	}

	/*
	 * Stop the driver
	 */
	if (!strcmp(argv[1], "stop")) {
		mb12xx::stop();
	}

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(argv[1], "test")) {
		mb12xx::test();
	}

	/*
	 * Reset the driver.
	 */
	if (!strcmp(argv[1], "reset")) {
		mb12xx::reset();
	}

	/*
	 * Print driver information.
	 */
	if (!strcmp(argv[1], "info") || !strcmp(argv[1], "status")) {
		mb12xx::info();
	}

	errx(1, "unrecognized command, try 'start', 'test', 'reset' or 'info'");
}
