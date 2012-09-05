/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: @author Lorenz Meier <lm@inf.ethz.ch>
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
 * @file sensors.cpp
 *
 * Sensor readout process.
 */

#include <nuttx/config.h>

#include <fcntl.h>
#include <poll.h>
#include <nuttx/analog/adc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include <arch/board/up_hrt.h>

#include <drivers/drv_accel.h>
#include <drivers/drv_gyro.h>
#include <drivers/drv_mag.h>
#include <drivers/drv_baro.h>

#include <arch/board/up_adc.h>

#include <systemlib/systemlib.h>
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <systemlib/perf_counter.h>

#include <uORB/uORB.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/rc_channels.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/parameter_update.h>

#define GYRO_HEALTH_COUNTER_LIMIT_ERROR 20   /* 40 ms downtime at 500 Hz update rate   */
#define ACC_HEALTH_COUNTER_LIMIT_ERROR  20   /* 40 ms downtime at 500 Hz update rate   */
#define MAGN_HEALTH_COUNTER_LIMIT_ERROR 100  /* 1000 ms downtime at 100 Hz update rate  */
#define BARO_HEALTH_COUNTER_LIMIT_ERROR 50   /* 500 ms downtime at 100 Hz update rate  */
#define ADC_HEALTH_COUNTER_LIMIT_ERROR  10   /* 100 ms downtime at 100 Hz update rate  */

#define GYRO_HEALTH_COUNTER_LIMIT_OK 5
#define ACC_HEALTH_COUNTER_LIMIT_OK  5
#define MAGN_HEALTH_COUNTER_LIMIT_OK 5
#define BARO_HEALTH_COUNTER_LIMIT_OK 5
#define ADC_HEALTH_COUNTER_LIMIT_OK  5

#define ADC_BATTERY_VOLATGE_CHANNEL  10

#define BAT_VOL_INITIAL 12.f
#define BAT_VOL_LOWPASS_1 0.99f
#define BAT_VOL_LOWPASS_2 0.01f
#define VOLTAGE_BATTERY_IGNORE_THRESHOLD_VOLTS 3.5f

#ifdef CONFIG_HRT_PPM
extern "C" {
	extern uint16_t ppm_buffer[];
	extern unsigned ppm_decoded_channels;
	extern uint64_t ppm_last_valid_decode;
}

/* PPM Settings */
#  define PPM_MIN 1000
#  define PPM_MAX 2000
/* Internal resolution is 10000 */
#  define PPM_SCALE 10000/((PPM_MAX-PPM_MIN)/2)
#  define PPM_MID (PPM_MIN+PPM_MAX)/2
#endif

/**
 * Sensor app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int sensors_main(int argc, char *argv[]);

class Sensors
{
public:
	/** 
	 * Constructor 
	 */
	Sensors();

	/**
	 * Destructor, also kills the sensors task.
	 */
	~Sensors();

	/**
	 * Start the sensors task.
	 *
	 * @return		OK on success.
	 */
	int		start();

private:	
	static const unsigned _rc_max_chan_count = 8;	/**< maximum number of r/c channels we handle */

	/* legacy sensor descriptors */
	int 		_fd_bma180;			/**< old accel driver */
	int 		_fd_gyro_l3gd20;		/**< old gyro driver */

#if CONFIG_HRT_PPM
	hrt_abstime	_ppm_last_valid;		/**< last time we got a valid ppm signal */

	/**
	 * Gather and publish PPM input data.
	 */
	void		ppm_poll();
#endif

	/* XXX should not be here - should be own driver */
	int 		_fd_adc;			/**< ADC driver handle */
	hrt_abstime	_last_adc;			/**< last time we took input from the ADC */

	bool 		_task_should_exit;		/**< if true, sensor task should exit */
	int 		_sensors_task;			/**< task handle for sensor task */

	bool		_hil_enabled;			/**< if true, HIL is active */
	bool		_publishing;			/**< if true, we are publishing sensor data */

	int		_gyro_sub;			/**< raw gyro data subscription */
	int		_accel_sub;			/**< raw accel data subscription */
	int		_mag_sub;			/**< raw mag data subscription */
	int		_baro_sub;			/**< raw baro data subscription */
	int		_vstatus_sub;			/**< vehicle status subscription */
	int 		_params_sub;			/**< notification of parameter updates */

	orb_advert_t	_sensor_pub;			/**< combined sensor data topic */
	orb_advert_t	_manual_control_pub;		/**< manual control signal topic */
	orb_advert_t	_rc_pub;			/**< raw r/c control topic */

	perf_counter_t	_loop_perf;			/**< loop performance counter */

	struct rc_channels_s _rc;			/**< r/c channel data */

	struct {
		float min[_rc_max_chan_count];
		float trim[_rc_max_chan_count];
		float max[_rc_max_chan_count];
		float rev[_rc_max_chan_count];

		float gyro_offset[3];
		float mag_offset[3];
		float acc_offset[3];

		int rc_type;

		int rc_map_roll;
		int rc_map_pitch;
		int rc_map_yaw;
		int rc_map_throttle;
		int rc_map_mode_sw;

		float battery_voltage_scaling;
	}		_parameters;			/**< local copies of interesting parameters */

	struct {
		param_t min[_rc_max_chan_count];
		param_t trim[_rc_max_chan_count];
		param_t max[_rc_max_chan_count];
		param_t rev[_rc_max_chan_count];
		param_t rc_type;

		param_t gyro_offset[3];
		param_t mag_offset[3];
		param_t acc_offset[3];

		param_t rc_map_roll;
		param_t rc_map_pitch;
		param_t rc_map_yaw;
		param_t rc_map_throttle;
		param_t rc_map_mode_sw;

		param_t battery_voltage_scaling;
	}		_parameter_handles;		/**< handles for interesting parameters */


	/**
	 * Update our local parameter cache.
	 */
	int		parameters_update();

	/**
	 * Do accel-related initialisation.
	 */
	void		accel_init();

	/**
	 * Do gyro-related initialisation.
	 */
	void		gyro_init();

	/**
	 * Do mag-related initialisation.
	 */
	void		mag_init();

	/**
	 * Do baro-related initialisation.
	 */
	void		baro_init();

	/**
	 * Do adc-related initialisation.
	 */
	void		adc_init();

	/**
	 * Poll the accelerometer for updated data.
	 *
	 * @param raw			Combined sensor data structure into which
	 *				data should be returned.
	 */
	void		accel_poll(struct sensor_combined_s &raw);

	/**
	 * Poll the gyro for updated data.
	 *
	 * @param raw			Combined sensor data structure into which
	 *				data should be returned.
	 */
	void		gyro_poll(struct sensor_combined_s &raw);

	/**
	 * Poll the magnetometer for updated data.
	 *
	 * @param raw			Combined sensor data structure into which
	 *				data should be returned.
	 */
	void		mag_poll(struct sensor_combined_s &raw);

	/**
	 * Poll the barometer for updated data.
	 *
	 * @param raw			Combined sensor data structure into which
	 *				data should be returned.
	 */
	void		baro_poll(struct sensor_combined_s &raw);

	/**
	 * Check for changes in vehicle status.
	 */
	void		vehicle_status_poll();

	/**
	 * Check for changes in parameters.
	 */
	void 		parameter_update_poll(bool forced = false);

	/**
	 * Poll the ADC and update readings to suit.
	 *
	 * @param raw			Combined sensor data structure into which
	 *				data should be returned.
	 */
	void		adc_poll(struct sensor_combined_s &raw);

	/**
	 * Shim for calling task_main from task_create.
	 */
	static void	task_main_trampoline(int argc, char *argv[]);

	/**
	 * Main sensor collection task.
	 */
	void		task_main() __attribute__((noreturn));
};

namespace sensors
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

Sensors	*g_sensors;
}

Sensors::Sensors() :
	_fd_bma180(-1),
	_fd_gyro_l3gd20(-1),
	_ppm_last_valid(0),

	_fd_adc(-1),
	_last_adc(0),

	_task_should_exit(false),
	_sensors_task(-1),
	_hil_enabled(false),
	_publishing(true),

	/* subscriptions */
	_gyro_sub(-1),
	_accel_sub(-1),
	_mag_sub(-1),
	_baro_sub(-1),
	_vstatus_sub(-1),
	_params_sub(-1),

	/* publications */
	_sensor_pub(-1),
	_manual_control_pub(-1),
	_rc_pub(-1),

	/* performance counters */
	_loop_perf(perf_alloc(PC_ELAPSED, "sensor task update"))
{
	_parameter_handles.min[0] = param_find("RC1_MIN");
	_parameter_handles.max[0] = param_find("RC1_MAX");
	_parameter_handles.trim[0] = param_find("RC1_TRIM");
	_parameter_handles.rev[0] = param_find("RC1_REV");

	/* basic r/c parameters */
	for (unsigned i = 1; i < _rc_max_chan_count; i++) {
		char nbuf[16];

		/* min values */
		sprintf(nbuf, "RC%d_MIN", i + 1);
		_parameter_handles.min[i] = param_find(nbuf);

		/* trim values */
		sprintf(nbuf, "RC%d_TRIM", i + 1);
		_parameter_handles.trim[i] = param_find(nbuf);

		/* max values */
		sprintf(nbuf, "RC%d_MAX", i + 1);
		_parameter_handles.max[i] = param_find(nbuf);

		/* channel reverse */
		sprintf(nbuf, "RC%d_REV", i + 1);
		_parameter_handles.rev[i] = param_find(nbuf);
	}

	_parameter_handles.rc_type = param_find("RC_TYPE");

	_parameter_handles.rc_map_roll 	= param_find("RC_MAP_ROLL");
	_parameter_handles.rc_map_pitch = param_find("RC_MAP_PITCH");
	_parameter_handles.rc_map_yaw 	= param_find("RC_MAP_YAW");
	_parameter_handles.rc_map_throttle = param_find("RC_MAP_THROTTLE");
	_parameter_handles.rc_map_mode_sw = param_find("RC_MAP_MODE_SW");

	/* gyro offsets */
	_parameter_handles.gyro_offset[0] = param_find("SENSOR_GYRO_XOFF");
	_parameter_handles.gyro_offset[1] = param_find("SENSOR_GYRO_YOFF");
	_parameter_handles.gyro_offset[2] = param_find("SENSOR_GYRO_ZOFF");

	/* accel offsets */
	_parameter_handles.acc_offset[0] = param_find("SENSOR_ACC_XOFF");
	_parameter_handles.acc_offset[1] = param_find("SENSOR_ACC_YOFF");
	_parameter_handles.acc_offset[2] = param_find("SENSOR_ACC_ZOFF");

	/* mag offsets */
	_parameter_handles.mag_offset[0] = param_find("SENSOR_MAG_XOFF");
	_parameter_handles.mag_offset[1] = param_find("SENSOR_MAG_YOFF");
	_parameter_handles.mag_offset[2] = param_find("SENSOR_MAG_ZOFF");

	_parameter_handles.battery_voltage_scaling = param_find("BAT_V_SCALING");

	/* fetch initial parameter values */
	parameters_update();
}

Sensors::~Sensors()
{
	if (_sensors_task != -1) {

		/* task wakes up every 100ms or so at the longest */
		_task_should_exit = true;

		/* wait for a second for the task to quit at our request */
		unsigned i = 0;
		do {
			/* wait 20ms */
			usleep(20000);

			/* if we have given up, kill it */
			if (++i > 50) {
				task_delete(_sensors_task);
				break;
			}
		} while (_sensors_task != -1);
	}

	sensors::g_sensors = nullptr;
}

int
Sensors::parameters_update()
{
	const unsigned int nchans = 8;

	/* rc values */
	for (unsigned int i = 0; i < nchans; i++) {

		if (param_get(_parameter_handles.min[i], &(_parameters.min[i])) != OK) {
			warnx("Failed getting min for chan %d", i);
		}
		if (param_get(_parameter_handles.trim[i], &(_parameters.trim[i])) != OK) {
			warnx("Failed getting trim for chan %d", i);
		}
		if (param_get(_parameter_handles.max[i], &(_parameters.max[i])) != OK) {
			warnx("Failed getting max for chan %d", i);
		}
		if (param_get(_parameter_handles.rev[i], &(_parameters.rev[i])) != OK) {
			warnx("Failed getting rev for chan %d", i);
		}

		_rc.chan[i].scaling_factor = (1.0f / ((_parameters.max[i] - _parameters.min[i]) / 2.0f) * _parameters.rev[i]);

		/* handle blowup in the scaling factor calculation */
		if (isnan(_rc.chan[i].scaling_factor) || isinf(_rc.chan[i].scaling_factor)) {
			_rc.chan[i].scaling_factor = 0;
		}

		_rc.chan[i].mid = _parameters.trim[i];
	}

	/* update RC function mappings */
	_rc.function[0] = _parameters.rc_map_throttle - 1;
	_rc.function[1] = _parameters.rc_map_roll - 1;
	_rc.function[2] = _parameters.rc_map_pitch - 1;
	_rc.function[3] = _parameters.rc_map_yaw - 1;
	_rc.function[4] = _parameters.rc_map_mode_sw - 1;

	/* remote control type */
	if (param_get(_parameter_handles.rc_type, &(_parameters.rc_type)) != OK) {
		warnx("Failed getting remote control type");
	}

	/* channel mapping */
	if (param_get(_parameter_handles.rc_map_roll, &(_parameters.rc_map_roll)) != OK) {
		warnx("Failed getting roll chan index");
	}
	if (param_get(_parameter_handles.rc_map_pitch, &(_parameters.rc_map_pitch)) != OK) {
		warnx("Failed getting pitch chan index");
	}
	if (param_get(_parameter_handles.rc_map_yaw, &(_parameters.rc_map_yaw)) != OK) {
		warnx("Failed getting yaw chan index");
	}
	if (param_get(_parameter_handles.rc_map_throttle, &(_parameters.rc_map_throttle)) != OK) {
		warnx("Failed getting throttle chan index");
	}
	if (param_get(_parameter_handles.rc_map_mode_sw, &(_parameters.rc_map_mode_sw)) != OK) {
		warnx("Failed getting mode sw chan index");
	}

	/* gyro offsets */
	param_get(_parameter_handles.gyro_offset[0], &(_parameters.gyro_offset[0]));
	param_get(_parameter_handles.gyro_offset[1], &(_parameters.gyro_offset[1]));
	param_get(_parameter_handles.gyro_offset[2], &(_parameters.gyro_offset[2]));

	/* accel offsets */
	param_get(_parameter_handles.acc_offset[0], &(_parameters.acc_offset[0]));
	param_get(_parameter_handles.acc_offset[1], &(_parameters.acc_offset[1]));
	param_get(_parameter_handles.acc_offset[2], &(_parameters.acc_offset[2]));

	/* mag offsets */
	param_get(_parameter_handles.mag_offset[0], &(_parameters.mag_offset[0]));
	param_get(_parameter_handles.mag_offset[1], &(_parameters.mag_offset[1]));
	param_get(_parameter_handles.mag_offset[2], &(_parameters.mag_offset[2]));

	/* scaling of ADC ticks to battery voltage */
	if (param_get(_parameter_handles.battery_voltage_scaling, &(_parameters.battery_voltage_scaling)) != OK) {
		warnx("Failed updating voltage scaling param");
	}

	return OK;
}

void
Sensors::accel_init()
{
	int	fd;

	fd = open(ACCEL_DEVICE_PATH, 0);
	if (fd < 0) {
		warn("%s", ACCEL_DEVICE_PATH);

		/* fall back to bma180 here (new driver would be better...) */
		_fd_bma180 = open("/dev/bma180", O_RDONLY);
		if (_fd_bma180 < 0) {
			warn("/dev/bma180");
			errx(1, "FATAL: no accelerometer found");
		}
	
		/* discard first (junk) reading */
		int16_t junk_buf[3];
		read(_fd_bma180, junk_buf, sizeof(junk_buf));

		warnx("using BMA180");
	} else {
		/* set the accel internal sampling rate up to at leat 500Hz */
		if (OK != ioctl(fd, ACCELIOCSSAMPLERATE, 500))
			warn("WARNING: failed to set minimum 500Hz sample rate for accel");

		/* set the driver to poll at 500Hz */
		if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 500))
			warn("WARNING: failed to set 500Hz poll rate for accel");

		warnx("using system accel");
		close(fd);
	}
}

void
Sensors::gyro_init()
{
	int	fd;

	fd = open(GYRO_DEVICE_PATH, 0);
	if (fd < 0) {
		warn("%s", GYRO_DEVICE_PATH);

		/* fall back to bma180 here (new driver would be better...) */
		_fd_gyro_l3gd20 = open("/dev/l3gd20", O_RDONLY);
		if (_fd_gyro_l3gd20 < 0) {
			warn("/dev/l3gd20");
			errx(1, "FATAL: no gyro found");
		}

		/* discard first (junk) reading */
		int16_t junk_buf[3];
		read(_fd_gyro_l3gd20, junk_buf, sizeof(junk_buf));

		warn("using L3GD20");
	} else {
		/* set the gyro internal sampling rate up to at leat 500Hz */
		if (OK != ioctl(fd, GYROIOCSSAMPLERATE, 500))
			warn("WARNING: failed to set minimum 500Hz sample rate for gyro");

		/* set the driver to poll at 500Hz */
		if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 500))
			warn("WARNING: failed to set 500Hz poll rate for gyro");

		warnx("using system gyro");
		close(fd);
	}
}

void
Sensors::mag_init()
{
	int	fd;

	fd = open(MAG_DEVICE_PATH, 0);
	if (fd < 0) {
		warn("%s", MAG_DEVICE_PATH);
		errx(1, "FATAL: no magnetometer found");
	}

	/* set the mag internal poll rate to at least 150Hz */
	if (OK != ioctl(fd, MAGIOCSSAMPLERATE, 150))
		warn("WARNING: failed to set minimum 150Hz sample rate for mag");

	/* set the driver to poll at 150Hz */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 150))
		warn("WARNING: failed to set 150Hz poll rate for mag");

	close(fd);
}

void
Sensors::baro_init()
{
	int	fd;

	fd = open(BARO_DEVICE_PATH, 0);
	if (fd < 0) {
		warn("%s", BARO_DEVICE_PATH);
		errx(1, "FATAL: no barometer found");
	}

	/* set the driver to poll at 150Hz */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 150))
		warn("WARNING: failed to set 150Hz poll rate for baro");

	close(fd);
}

void
Sensors::adc_init()
{

	_fd_adc = open("/dev/adc0", O_RDONLY | O_NONBLOCK);
	if (_fd_adc < 0) {
		warn("/dev/adc0");
		errx(1, "FATAL: no ADC found");
	}
}

void
Sensors::accel_poll(struct sensor_combined_s &raw)
{
	struct accel_report	accel_report;

	if (_fd_bma180 >= 0) {
		/* do ORB emulation for BMA180 */
		int16_t		buf[3];

		read(_fd_bma180, buf, sizeof(buf));

		accel_report.timestamp = hrt_absolute_time();

		accel_report.x_raw = (buf[1] == -32768) ? 32767 : -buf[1];
		accel_report.y_raw = buf[0];
		accel_report.z_raw = buf[2];

		const float range_g = 4.0f;
		/* scale from 14 bit to m/s2 */
		accel_report.x = (((accel_report.x_raw - _parameters.acc_offset[0]) * range_g) / 8192.0f) / 9.81f;
		accel_report.y = (((accel_report.y_raw - _parameters.acc_offset[0]) * range_g) / 8192.0f) / 9.81f;
		accel_report.z = (((accel_report.z_raw - _parameters.acc_offset[0]) * range_g) / 8192.0f) / 9.81f;

	} else {
		orb_copy(ORB_ID(sensor_accel), _accel_sub, &accel_report);
	}

	raw.accelerometer_m_s2[0] = accel_report.x;
	raw.accelerometer_m_s2[1] = accel_report.y;
	raw.accelerometer_m_s2[2] = accel_report.z;

	raw.accelerometer_raw[0] = accel_report.x_raw;
	raw.accelerometer_raw[1] = accel_report.y_raw;
	raw.accelerometer_raw[2] = accel_report.z_raw;

	raw.accelerometer_raw_counter++;
}

void
Sensors::gyro_poll(struct sensor_combined_s &raw)
{
	struct gyro_report	gyro_report;

	if (_fd_gyro_l3gd20 >= 0) {
		/* do ORB emulation for L3GD20 */
		int16_t		buf[3];

		read(_fd_gyro_l3gd20, buf, sizeof(buf));

		gyro_report.timestamp = hrt_absolute_time();

		gyro_report.x_raw = buf[1];
		gyro_report.y_raw = ((buf[0] == -32768) ? 32767 : -buf[0]);
		gyro_report.z_raw = buf[2];

		/* scaling calculated as: raw * (1/(32768*(500/180*PI))) */
		gyro_report.x = (gyro_report.x_raw - _parameters.gyro_offset[0]) * 0.000266316109f;
		gyro_report.y = (gyro_report.y_raw - _parameters.gyro_offset[1]) * 0.000266316109f;
		gyro_report.z = (gyro_report.z_raw - _parameters.gyro_offset[2]) * 0.000266316109f;

	} else {
		orb_copy(ORB_ID(sensor_gyro), _gyro_sub, &gyro_report);

		raw.gyro_rad_s[0] = gyro_report.x;
		raw.gyro_rad_s[1] = gyro_report.y;
		raw.gyro_rad_s[2] = gyro_report.z;

		raw.gyro_raw[0] = gyro_report.x_raw;
		raw.gyro_raw[1] = gyro_report.y_raw;
		raw.gyro_raw[2] = gyro_report.z_raw;

		raw.gyro_raw_counter++;		
	}
}

void
Sensors::mag_poll(struct sensor_combined_s &raw)
{
	struct mag_report	mag_report;

	orb_copy(ORB_ID(sensor_mag), _mag_sub, &mag_report);

	raw.magnetometer_ga[0] = mag_report.x;
	raw.magnetometer_ga[1] = mag_report.y;
	raw.magnetometer_ga[2] = mag_report.z;

	raw.magnetometer_raw[0] = mag_report.x_raw;
	raw.magnetometer_raw[1] = mag_report.y_raw;
	raw.magnetometer_raw[2] = mag_report.z_raw;
	
	raw.magnetometer_raw_counter++;
}

void
Sensors::baro_poll(struct sensor_combined_s &raw)
{
	struct baro_report	baro_report;

	orb_copy(ORB_ID(sensor_baro), _baro_sub, &baro_report);

	raw.baro_pres_mbar = baro_report.pressure; // Pressure in mbar
	raw.baro_alt_meter = baro_report.altitude; // Altitude in meters
	raw.baro_temp_celcius = baro_report.temperature; // Temperature in degrees celcius

	raw.baro_raw_counter++;
}

void
Sensors::vehicle_status_poll()
{
	struct vehicle_status_s vstatus;
	bool vstatus_updated;

	/* Check HIL state if vehicle status has changed */
	orb_check(_vstatus_sub, &vstatus_updated);
	if (vstatus_updated) {

		orb_copy(ORB_ID(vehicle_status), _vstatus_sub, &vstatus);

		/* switching from non-HIL to HIL mode */
		//printf("[sensors] Vehicle mode: %i \t AND: %i, HIL: %i\n", vstatus.mode, vstatus.mode & VEHICLE_MODE_FLAG_HIL_ENABLED, hil_enabled);
		if (vstatus.flag_hil_enabled && !_hil_enabled) {
			_hil_enabled = true;
			_publishing = false;

			/* switching from HIL to non-HIL mode */

		} else if (!_publishing && !_hil_enabled) {
			_hil_enabled = false;
			_publishing = true;
		}
	}
}

void
Sensors::parameter_update_poll(bool forced)
{
	bool param_updated;

	/* Check if any parameter has changed */
	orb_check(_params_sub, &param_updated);

	if (param_updated || forced)
	{
		/* read from param to clear updated flag */
		struct parameter_update_s update;
		orb_copy(ORB_ID(parameter_update), _params_sub, &update);

		/* update parameters */
		parameters_update();

		/* update sensor offsets */
		int fd = open(GYRO_DEVICE_PATH, 0);
		struct gyro_scale gscale = { 
			_parameters.gyro_offset[0],
			1.0f,
			_parameters.gyro_offset[1],
			1.0f,
			_parameters.gyro_offset[2],
			1.0f,
		};
		if (OK != ioctl(fd, GYROIOCSSCALE, (long unsigned int)&gscale))
			warn("WARNING: failed to set scale / offsets for gyro");
		close(fd);

		fd = open(ACCEL_DEVICE_PATH, 0);
		struct accel_scale ascale = {
			_parameters.acc_offset[0],
			1.0f,
			_parameters.acc_offset[1],
			1.0f,
			_parameters.acc_offset[2],
			1.0f,
		};
		if (OK != ioctl(fd, ACCELIOCSSCALE, (long unsigned int)&ascale))
			warn("WARNING: failed to set scale / offsets for accel");
		close(fd);

		fd = open(MAG_DEVICE_PATH, 0);
		struct mag_scale mscale = {
			_parameters.mag_offset[0],
			1.0f,
			_parameters.mag_offset[1],
			1.0f,
			_parameters.mag_offset[2],
			1.0f,
		};
		if (OK != ioctl(fd, MAGIOCSSCALE, (long unsigned int)&mscale))
			warn("WARNING: failed to set scale / offsets for mag");
		close(fd);

#if 0
		printf("CH0: RAW MAX: %d MIN %d S: %d MID: %d FUNC: %d\n",  (int)_parameters.max[0], (int)_parameters.min[0], (int)(_rc.chan[0].scaling_factor*10000), (int)(_rc.chan[0].mid), (int)_rc.function[0]);
		printf("CH1: RAW MAX: %d MIN %d S: %d MID: %d FUNC: %d\n",  (int)_parameters.max[1], (int)_parameters.min[1], (int)(_rc.chan[1].scaling_factor*10000), (int)(_rc.chan[1].mid), (int)_rc.function[1]);
		printf("MAN: %d %d\n", (int)(_rc.chan[0].scaled*100), (int)(_rc.chan[1].scaled*100));
		fflush(stdout);
		usleep(5000);
#endif
	}	
}

void
Sensors::adc_poll(struct sensor_combined_s &raw)
{
	#pragma pack(push,1)
	struct adc_msg4_s {
		uint8_t      am_channel1;	/**< The 8-bit ADC Channel 1 */
		int32_t      am_data1;		/**< ADC convert result 1 (4 bytes) */
		uint8_t      am_channel2;	/**< The 8-bit ADC Channel 2 */
		int32_t      am_data2;		/**< ADC convert result 2 (4 bytes) */
		uint8_t      am_channel3;	/**< The 8-bit ADC Channel 3 */
		int32_t      am_data3;		/**< ADC convert result 3 (4 bytes) */
		uint8_t      am_channel4;	/**< The 8-bit ADC Channel 4 */
		int32_t      am_data4;		/**< ADC convert result 4 (4 bytes) */
	} buf_adc;
	#pragma pack(pop)

	if (hrt_absolute_time() - _last_adc >= 10000) {
		read(_fd_adc, &buf_adc, sizeof(buf_adc));

		if (ADC_BATTERY_VOLATGE_CHANNEL == buf_adc.am_channel1) {
			/* Voltage in volts */
			raw.battery_voltage_v = (BAT_VOL_LOWPASS_1 * (raw.battery_voltage_v + BAT_VOL_LOWPASS_2 * (buf_adc.am_data1 * _parameters.battery_voltage_scaling)));

			if ((buf_adc.am_data1 * _parameters.battery_voltage_scaling) < VOLTAGE_BATTERY_IGNORE_THRESHOLD_VOLTS) {
				raw.battery_voltage_valid = false;
				raw.battery_voltage_v = 0.f;

			} else {
				raw.battery_voltage_valid = true;
			}

			raw.battery_voltage_counter++;
		}
		_last_adc = hrt_absolute_time();
	}
}

#if CONFIG_HRT_PPM
void
Sensors::ppm_poll()
{
	struct manual_control_setpoint_s manual_control;

	/* check to see whether a new frame has been decoded */
	if (_ppm_last_valid == ppm_last_valid_decode)
		return;
	/* require at least two chanels to consider the signal valid */
	if (ppm_decoded_channels < 4)
		return;

	/* we are accepting this decode */
	_ppm_last_valid = ppm_last_valid_decode;

	/* Read out values from HRT */
	for (unsigned int i = 0; i < ppm_decoded_channels; i++) {
		_rc.chan[i].raw = ppm_buffer[i];
		/* Set the range to +-, then scale up */
		_rc.chan[i].scale = (ppm_buffer[i] - _rc.chan[i].mid) * _rc.chan[i].scaling_factor * 10000;
		_rc.chan[i].scaled = (ppm_buffer[i] - _rc.chan[i].mid) * _rc.chan[i].scaling_factor;
	}

	_rc.chan_count = ppm_decoded_channels;
	_rc.timestamp = ppm_last_valid_decode;

	/* roll input */
	manual_control.roll = _rc.chan[_rc.function[ROLL]].scaled;
	if (manual_control.roll < -1.0f) manual_control.roll = -1.0f;
	if (manual_control.roll >  1.0f) manual_control.roll =  1.0f;

	/* pitch input */
	manual_control.pitch = _rc.chan[_rc.function[PITCH]].scaled;
	if (manual_control.pitch < -1.0f) manual_control.pitch = -1.0f;
	if (manual_control.pitch >  1.0f) manual_control.pitch =  1.0f;

	/* yaw input */
	manual_control.yaw = _rc.chan[_rc.function[YAW]].scaled;
	if (manual_control.yaw < -1.0f) manual_control.yaw = -1.0f;
	if (manual_control.yaw >  1.0f) manual_control.yaw =  1.0f;
	
	/* throttle input */
	manual_control.throttle = _rc.chan[_rc.function[THROTTLE]].scaled/2.0f;
	if (manual_control.throttle < 0.0f) manual_control.throttle = 0.0f;
	if (manual_control.throttle > 1.0f) manual_control.throttle = 1.0f;

	/* mode switch input */
	manual_control.override_mode_switch = _rc.chan[_rc.function[OVERRIDE]].scaled;
	if (manual_control.override_mode_switch < -1.0f) manual_control.override_mode_switch = -1.0f;
	if (manual_control.override_mode_switch >  1.0f) manual_control.override_mode_switch =  1.0f;

	orb_publish(ORB_ID(rc_channels), _rc_pub, &_rc);
	orb_publish(ORB_ID(manual_control_setpoint), _manual_control_pub, &manual_control);
}
#endif

void
Sensors::task_main_trampoline(int argc, char *argv[])
{
	sensors::g_sensors->task_main();
}

void
Sensors::task_main()
{

	/* inform about start */
	printf("[sensors] Initializing..\n");
	fflush(stdout);

	/* start individual sensors */
	accel_init();
	gyro_init();
	mag_init();
	baro_init();
	adc_init();

	/*
	 * do subscriptions
	 */
	_gyro_sub = orb_subscribe(ORB_ID(sensor_gyro));
	_accel_sub = orb_subscribe(ORB_ID(sensor_accel));
	_mag_sub = orb_subscribe(ORB_ID(sensor_mag));
	_baro_sub = orb_subscribe(ORB_ID(sensor_baro));
	_vstatus_sub = orb_subscribe(ORB_ID(vehicle_status));
	_params_sub = orb_subscribe(ORB_ID(parameter_update));

	/* rate limit vehicle status updates to 5Hz */
	orb_set_interval(_vstatus_sub, 200);

	/*
	 * do advertisements
	 */
	struct sensor_combined_s raw;
	raw.timestamp = hrt_absolute_time();
	raw.battery_voltage_v = BAT_VOL_INITIAL;
	raw.adc_voltage_v[0] = 0.9f;
	raw.adc_voltage_v[1] = 0.0f;
	raw.adc_voltage_v[2] = 0.0f;
	raw.battery_voltage_counter = 0;
	raw.battery_voltage_valid = false;

	/* get a set of initial values */
	accel_poll(raw);
	gyro_poll(raw);
	mag_poll(raw);
	baro_poll(raw);

	parameter_update_poll(true /* forced */);

	/* advertise the sensor_combined topic and make the initial publication */
	_sensor_pub = orb_advertise(ORB_ID(sensor_combined), &raw);

	/* advertise the manual_control topic */
	{
		struct manual_control_setpoint_s manual_control;
		manual_control.mode = ROLLPOS_PITCHPOS_YAWRATE_THROTTLE;
		manual_control.roll = 0.0f;
		manual_control.pitch = 0.0f;
		manual_control.yaw = 0.0f;
		manual_control.throttle = 0.0f;

		_manual_control_pub = orb_advertise(ORB_ID(manual_control_setpoint), &manual_control);
	}

	/* advertise the rc topic */
	{
		struct rc_channels_s rc;
		memset(&rc, 0, sizeof(rc));
		_rc_pub = orb_advertise(ORB_ID(rc_channels), &rc);
	}

	/* wakeup source(s) */
	struct pollfd fds[1];

	/* use the gyro to pace output - XXX BROKEN if we are using the L3GD20 */
	fds[0].fd = _gyro_sub;
	fds[0].events = POLLIN;

	while (!_task_should_exit) {
		
		/* wait for up to 500ms for data */
		int pret = poll(&fds[0], (sizeof(fds) / sizeof(fds[0])), 100);

		/* timed out - periodic check for _task_should_exit, etc. */
		if (pret == 0)
			continue;

		/* this is undesirable but not much we can do - might want to flag unhappy status */
		if (pret < 0) {
			warn("poll error %d, %d", pret, errno);
			continue;
		}

		perf_begin(_loop_perf);

		/* check vehicle status for changes to publication state */
		vehicle_status_poll();

		/* check parameters for updates */
		parameter_update_poll();

		/* store the time closest to all measurements (this is bogus, sensor timestamps should be propagated...) */
		raw.timestamp = hrt_absolute_time();

		/* copy most recent sensor data */
		accel_poll(raw);
		gyro_poll(raw);
		mag_poll(raw);
		baro_poll(raw);

		/* check battery voltage */
		adc_poll(raw);

		/* Inform other processes that new data is available to copy */
		if (_publishing)
			orb_publish(ORB_ID(sensor_combined), _sensor_pub, &raw);

#ifdef CONFIG_HRT_PPM
		/* Look for new r/c input data */
		ppm_poll();
#endif

		perf_end(_loop_perf);
	}

	printf("[sensors] exiting.\n");

	_sensors_task = -1;
	_exit(0);
}

int
Sensors::start()
{
	ASSERT(_sensors_task == -1);

	/* start the task */
	_sensors_task = task_create("sensor_task",
				    SCHED_PRIORITY_MAX - 5,
				    4096,	/* XXX may be excesssive */
				    (main_t)&Sensors::task_main_trampoline,
				    nullptr);

	if (_sensors_task < 0) {
		warn("task start failed");
		return -errno;
	}
	return OK;
}

int sensors_main(int argc, char *argv[])
{
	if (argc < 1)
		errx(1, "usage: sensors {start|stop|status}");

	if (!strcmp(argv[1], "start")) {

		if (sensors::g_sensors != nullptr)
			errx(1, "sensors task already running");

		sensors::g_sensors = new Sensors;
		if (sensors::g_sensors == nullptr)
			errx(1, "sensors task alloc failed");

		if (OK != sensors::g_sensors->start()) {
			delete sensors::g_sensors;
			sensors::g_sensors = nullptr;
			err(1, "sensors task start failed");
		}
		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		if (sensors::g_sensors == nullptr)
			errx(1, "sensors task not running");
		delete sensors::g_sensors;
		sensors::g_sensors = nullptr;
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (sensors::g_sensors) {
			errx(0, "task is running");
		} else {
			errx(1, "task is not running");
		}
	}

	errx(1, "unrecognized command");
}

