/****************************************************************************
 *
 *   Copyright (c) 2014, 2015 PX4 Development Team. All rights reserved.
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
 * @file LidarLiteI2C.cpp
 * @author Allyson Kreft
 *
 * Driver for the PulsedLight Lidar-Lite range finders connected via I2C.
 */

#include "LidarLiteI2C.h"
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdio.h>
#include <drivers/drv_hrt.h>

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

LidarLiteI2C::LidarLiteI2C(int bus, const char *path, int address) :
	I2C("LL40LS", path, bus, address, 100000),
	_work{},
	_reports(nullptr),
	_sensor_ok(false),
	_collect_phase(false),
	_class_instance(-1),
	_orb_class_instance(-1),
	_distance_sensor_topic(nullptr),
	_sample_perf(perf_alloc(PC_ELAPSED, "ll40ls_i2c_read")),
	_comms_errors(perf_alloc(PC_COUNT, "ll40ls_i2c_comms_errors")),
	_buffer_overflows(perf_alloc(PC_COUNT, "ll40ls_buffer_i2c_overflows")),
	_sensor_resets(perf_alloc(PC_COUNT, "ll40ls_i2c_resets")),
	_sensor_zero_resets(perf_alloc(PC_COUNT, "ll40ls_i2c_zero_resets")),
	_last_distance(0),
	_zero_counter(0),
	_acquire_time_usec(0),
	_pause_measurements(false),
	_bus(bus)
{
	// up the retries since the device misses the first measure attempts
	_retries = 3;

	// enable debug() calls
	_debug_enabled = false;

	// work_cancel in the dtor will explode if we don't do this...
	memset(&_work, 0, sizeof(_work));
}

LidarLiteI2C::~LidarLiteI2C()
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

	// free perf counters
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_buffer_overflows);
	perf_free(_sensor_resets);
	perf_free(_sensor_zero_resets);
}

int LidarLiteI2C::init()
{
	int ret = ERROR;

	/* do I2C init (and probe) first */
	if (I2C::init() != OK) {
		goto out;
	}

	/* allocate basic report buffers */
	_reports = new ringbuffer::RingBuffer(2, sizeof(struct distance_sensor_s));

	if (_reports == nullptr) {
		goto out;
	}

	_class_instance = register_class_devname(RANGE_FINDER_BASE_DEVICE_PATH);

	if (_class_instance == CLASS_DEVICE_PRIMARY) {
		/* get a publish handle on the range finder topic */
		struct distance_sensor_s ds_report;
		measure();
		_reports->get(&ds_report);
		_distance_sensor_topic = orb_advertise_multi(ORB_ID(distance_sensor), &ds_report,
							     &_orb_class_instance, ORB_PRIO_LOW);

		if (_distance_sensor_topic == nullptr) {
			debug("failed to create distance_sensor object. Did you start uOrb?");
		}
	}

	ret = OK;
	/* sensor is ok, but we don't really know if it is within range */
	_sensor_ok = true;
out:
	return ret;
}

int LidarLiteI2C::read_reg(uint8_t reg, uint8_t &val)
{
	return transfer(&reg, 1, &val, 1);
}

int LidarLiteI2C::probe()
{
	// cope with both old and new I2C bus address
	const uint8_t addresses[2] = {LL40LS_BASEADDR, LL40LS_BASEADDR_OLD};

	// more retries for detection
	_retries = 10;

	for (uint8_t i = 0; i < sizeof(addresses); i++) {
		uint8_t who_am_i = 0, max_acq_count = 0;

		// set the I2C bus address
		set_address(addresses[i]);

		/* register 2 defaults to 0x80. If this matches it is
		   almost certainly a ll40ls */
		if (read_reg(LL40LS_MAX_ACQ_COUNT_REG, max_acq_count) == OK && max_acq_count == 0x80) {
			// very likely to be a ll40ls. This is the
			// default max acquisition counter
			goto ok;
		}

		if (read_reg(LL40LS_WHO_AM_I_REG, who_am_i) == OK && who_am_i == LL40LS_WHO_AM_I_REG_VAL) {
			// it is responding correctly to a
			// WHO_AM_I. This works with older sensors (pre-production)
			goto ok;
		}

		debug("probe failed reg11=0x%02x reg2=0x%02x\n",
		      (unsigned)who_am_i,
		      (unsigned)max_acq_count);
	}

	// not found on any address
	return -EIO;

ok:
	_retries = 3;

	// reset the sensor to ensure it is in a known state with
	// correct settings
	return reset_sensor();
}

int LidarLiteI2C::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	switch (cmd) {
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

	default: {
			int result = LidarLite::ioctl(filp, cmd, arg);

			if (result == -EINVAL) {
				result = I2C::ioctl(filp, cmd, arg);
			}

			return result;
		}
	}
}

ssize_t LidarLiteI2C::read(struct file *filp, char *buffer, size_t buflen)
{
	unsigned count = buflen / sizeof(struct distance_sensor_s);
	struct distance_sensor_s *rbuf = reinterpret_cast<struct distance_sensor_s *>(buffer);
	int ret = 0;

	/* buffer must be large enough */
	if (count < 1) {
		return -ENOSPC;
	}

	/* if automatic measurement is enabled */
	if (getMeasureTicks() > 0) {

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
		usleep(LL40LS_CONVERSION_INTERVAL);

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

int LidarLiteI2C::measure()
{
	int ret;

	if (_pause_measurements) {
		// we are in print_registers() and need to avoid
		// acquisition to keep the I2C peripheral on the
		// sensor active
		return OK;
	}

	/*
	 * Send the command to begin a measurement.
	 */
	const uint8_t cmd[2] = { LL40LS_MEASURE_REG, LL40LS_MSRREG_ACQUIRE };
	ret = transfer(cmd, sizeof(cmd), nullptr, 0);

	if (OK != ret) {
		perf_count(_comms_errors);
		debug("i2c::transfer returned %d", ret);

		// if we are getting lots of I2C transfer errors try
		// resetting the sensor
		if (perf_event_count(_comms_errors) % 10 == 0) {
			perf_count(_sensor_resets);
			reset_sensor();
		}

		return ret;
	}

	// remember when we sent the acquire so we can know when the
	// acquisition has timed out
	_acquire_time_usec = hrt_absolute_time();
	ret = OK;

	return ret;
}

/*
  reset the sensor to power on defaults
 */
int LidarLiteI2C::reset_sensor()
{
	const uint8_t cmd[2] = { LL40LS_MEASURE_REG, LL40LS_MSRREG_RESET };
	int ret = transfer(cmd, sizeof(cmd), nullptr, 0);
	return ret;
}

/*
  dump sensor registers for debugging
 */
void LidarLiteI2C::print_registers()
{
	_pause_measurements = true;
	printf("ll40ls registers\n");
	// wait for a while to ensure the lidar is in a ready state
	usleep(50000);

	for (uint8_t reg = 0; reg <= 0x67; reg++) {
		uint8_t val = 0;
		int ret = transfer(&reg, 1, &val, 1);

		if (ret != OK) {
			printf("%02x:XX ", (unsigned)reg);

		} else {
			printf("%02x:%02x ", (unsigned)reg, (unsigned)val);
		}

		if (reg % 16 == 15) {
			printf("\n");
		}
	}

	printf("\n");
	_pause_measurements = false;
}

int LidarLiteI2C::collect()
{
	int ret = -EIO;

	/* read from the sensor */
	uint8_t val[2] = {0, 0};

	perf_begin(_sample_perf);

	// read the high and low byte distance registers
	uint8_t distance_reg = LL40LS_DISTHIGH_REG;
	ret = transfer(&distance_reg, 1, &val[0], sizeof(val));

	if (ret < 0) {
		if (hrt_absolute_time() - _acquire_time_usec > LL40LS_CONVERSION_TIMEOUT) {
			/*
			  NACKs from the sensor are expected when we
			  read before it is ready, so only consider it
			  an error if more than 100ms has elapsed.
			 */
			debug("error reading from sensor: %d", ret);
			perf_count(_comms_errors);

			if (perf_event_count(_comms_errors) % 10 == 0) {
				perf_count(_sensor_resets);
				reset_sensor();
			}
		}

		perf_end(_sample_perf);
		// if we are getting lots of I2C transfer errors try
		// resetting the sensor
		return ret;
	}

	uint16_t distance_cm = (val[0] << 8) | val[1];
	float distance_m = float(distance_cm) * 1e-2f;
	struct distance_sensor_s report;

	if (distance_cm == 0) {
		_zero_counter++;

		if (_zero_counter == 20) {
			/* we have had 20 zeros in a row - reset the
			   sensor. This is a known bad state of the
			   sensor where it returns 16 bits of zero for
			   the distance with a trailing NACK, and
			   keeps doing that even when the target comes
			   into a valid range.
			*/
			_zero_counter = 0;
			perf_end(_sample_perf);
			perf_count(_sensor_zero_resets);
			return reset_sensor();
		}

	} else {
		_zero_counter = 0;
	}

	_last_distance = distance_m;

	/* this should be fairly close to the end of the measurement, so the best approximation of the time */
	report.timestamp = hrt_absolute_time();
	report.current_distance = distance_m;
	report.min_distance = get_minimum_distance();
	report.max_distance = get_maximum_distance();
	report.covariance = 0.0f;
	/* the sensor is in fact a laser + sonar but there is no enum for this */
	report.type = distance_sensor_s::MAV_DISTANCE_SENSOR_LASER;
	report.orientation = 8;
	/* TODO: set proper ID */
	report.id = 0;

	/* publish it, if we are the primary */
	if (_distance_sensor_topic != nullptr) {
		orb_publish(ORB_ID(distance_sensor), _distance_sensor_topic, &report);
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

void LidarLiteI2C::start()
{
	/* reset the report ring and state machine */
	_collect_phase = false;
	_reports->flush();

	/* schedule a cycle to start things */
	work_queue(HPWORK, &_work, (worker_t)&LidarLiteI2C::cycle_trampoline, this, 1);
}

void LidarLiteI2C::stop()
{
	work_cancel(HPWORK, &_work);
}

void LidarLiteI2C::cycle_trampoline(void *arg)
{
	LidarLiteI2C *dev = (LidarLiteI2C *)arg;

	dev->cycle();
}

void LidarLiteI2C::cycle()
{
	/* collection phase? */
	if (_collect_phase) {

		/* try a collection */
		if (OK != collect()) {
			debug("collection error");

			/* if we've been waiting more than 200ms then
			   send a new acquire */
			if (hrt_absolute_time() - _acquire_time_usec > LL40LS_CONVERSION_TIMEOUT * 2) {
				_collect_phase = false;
			}

		} else {
			/* next phase is measurement */
			_collect_phase = false;

			/*
			 * Is there a collect->measure gap?
			 */
			if (getMeasureTicks() > USEC2TICK(LL40LS_CONVERSION_INTERVAL)) {

				/* schedule a fresh cycle call when we are ready to measure again */
				work_queue(HPWORK,
					   &_work,
					   (worker_t)&LidarLiteI2C::cycle_trampoline,
					   this,
					   getMeasureTicks() - USEC2TICK(LL40LS_CONVERSION_INTERVAL));

				return;
			}
		}
	}

	if (_collect_phase == false) {
		/* measurement phase */
		if (OK != measure()) {
			debug("measure error");

		} else {
			/* next phase is collection. Don't switch to
			   collection phase until we have a successful
			   acquire request I2C transfer */
			_collect_phase = true;
		}
	}

	/* schedule a fresh cycle call when the measurement is done */
	work_queue(HPWORK,
		   &_work,
		   (worker_t)&LidarLiteI2C::cycle_trampoline,
		   this,
		   USEC2TICK(LL40LS_CONVERSION_INTERVAL));
}

void LidarLiteI2C::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_buffer_overflows);
	perf_print_counter(_sensor_resets);
	perf_print_counter(_sensor_zero_resets);
	printf("poll interval:  %u ticks\n", getMeasureTicks());
	_reports->print_info("report queue");
	printf("distance: %ucm (0x%04x)\n",
	       (unsigned)_last_distance, (unsigned)_last_distance);
}
