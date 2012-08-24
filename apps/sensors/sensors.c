/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: @author Lorenz Meier <lm@inf.ethz.ch>
 *           @author Thomas Gubler <thomasgubler@student.ethz.ch>
 *           @author Julian Oes <joes@student.ethz.ch>
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
 * @file sensors.c
 * Sensor readout process.
 */

#include <nuttx/config.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <nuttx/analog/adc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <float.h>

#include <arch/board/up_hrt.h>
#include <arch/board/drv_bma180.h>
#include <arch/board/drv_l3gd20.h>

#include <drivers/drv_accel.h>
#include <drivers/drv_gyro.h>
#include <drivers/drv_mag.h>
#include <drivers/drv_baro.h>

#include <arch/board/up_adc.h>

#include <systemlib/systemlib.h>
#include <systemlib/param/param.h>

#include <uORB/uORB.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/rc_channels.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/vehicle_status.h>

#include "sensors.h"

#define errno *get_errno_ptr()

#define SENSOR_INTERVAL_MICROSEC 2000

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

/* PPM Settings */
#define PPM_MIN 1000
#define PPM_MAX 2000
/* Internal resolution is 10000 */
#define PPM_SCALE 10000/((PPM_MAX-PPM_MIN)/2)

#define PPM_MID (PPM_MIN+PPM_MAX)/2

static pthread_cond_t sensors_read_ready;
static pthread_mutex_t sensors_read_ready_mutex;

static int sensors_timer_loop_counter = 0;

/* File descriptors for all sensors */
static int fd_gyro = -1;
static int fd_gyro_l3gd20 = -1;

static bool thread_should_exit = false;
static bool thread_running = false;
static int sensors_task;

static int fd_bma180 = -1;
static int fd_magnetometer = -1;
static int fd_barometer = -1;
static int fd_adc = -1;
static int fd_accelerometer = -1;

/* Private functions declared static */
static void sensors_timer_loop(void *arg);

#ifdef CONFIG_HRT_PPM
extern uint16_t ppm_buffer[];
extern unsigned ppm_decoded_channels;
extern uint64_t ppm_last_valid_decode;
#endif

/* ORB topic publishing our results */
static orb_advert_t sensor_pub;

PARAM_DEFINE_FLOAT(SENSOR_GYRO_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_GYRO_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_GYRO_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(SENSOR_MAG_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_MAG_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_MAG_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(SENSOR_ACC_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_ACC_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_ACC_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(RC1_MIN, 1000.0f);
PARAM_DEFINE_FLOAT(RC1_TRIM, 1500.0f);
PARAM_DEFINE_FLOAT(RC1_MAX, 2000.0f);
PARAM_DEFINE_FLOAT(RC1_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC2_MIN, 1000);
PARAM_DEFINE_FLOAT(RC2_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC2_MAX, 2000);
PARAM_DEFINE_FLOAT(RC2_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC3_MIN, 1000);
PARAM_DEFINE_FLOAT(RC3_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC3_MAX, 2000);
PARAM_DEFINE_FLOAT(RC3_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC4_MIN, 1000);
PARAM_DEFINE_FLOAT(RC4_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC4_MAX, 2000);
PARAM_DEFINE_FLOAT(RC4_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC5_MIN, 1000);
PARAM_DEFINE_FLOAT(RC5_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC5_MAX, 2000);
PARAM_DEFINE_FLOAT(RC5_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC6_MIN, 1000);
PARAM_DEFINE_FLOAT(RC6_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC6_MAX, 2000);
PARAM_DEFINE_FLOAT(RC6_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC7_MIN, 1000);
PARAM_DEFINE_FLOAT(RC7_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC7_MAX, 2000);
PARAM_DEFINE_FLOAT(RC7_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC8_MIN, 1000);
PARAM_DEFINE_FLOAT(RC8_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC8_MAX, 2000);
PARAM_DEFINE_FLOAT(RC8_REV, 1.0f);

PARAM_DEFINE_INT32(RC_TYPE, 1); // 1 = FUTABA

PARAM_DEFINE_FLOAT(BAT_V_SCALING, -1.0f);

PARAM_DEFINE_INT32(RC_MAP_ROLL, 1);
PARAM_DEFINE_INT32(RC_MAP_PITCH, 2);
PARAM_DEFINE_INT32(RC_MAP_THROTTLE, 3);
PARAM_DEFINE_INT32(RC_MAP_YAW, 4);
PARAM_DEFINE_INT32(RC_MAP_MODE_SW, 5);

#define rc_max_chan_count 8

struct sensor_parameters {
	int min[rc_max_chan_count];
	int trim[rc_max_chan_count];
	int max[rc_max_chan_count];
	int rev[rc_max_chan_count];

	float gyro_offset[3];
	float mag_offset[3];
	float acc_offset[3];

	int rc_type;

	int rc_map_roll;
	int rc_map_pitch;
	int rc_map_yaw;
	int rc_map_throttle;
	int rc_map_mode_sw;

	int battery_voltage_scaling;
};

struct sensor_parameter_handles {
	param_t min[rc_max_chan_count];
	param_t trim[rc_max_chan_count];
	param_t max[rc_max_chan_count];
	param_t rev[rc_max_chan_count];
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
};

/**
 * Sensor app start / stop handling function
 *
 * @ingroup apps
 */
__EXPORT int sensors_main(int argc, char *argv[]);

/**
 * Sensor readout and publishing.
 * 
 * This function reads all onboard sensors and publishes the sensor_combined topic.
 *
 * @see sensor_combined_s
 */
int sensors_thread_main(int argc, char *argv[]);

/**
 * Print the usage
 */
static void usage(const char *reason);

/**
 * Initialize all parameter handles and values
 *
 */
static int parameters_init(struct sensor_parameter_handles *h);

/**
 * Update all parameters
 *
 */
static int parameters_update(const struct sensor_parameter_handles *h, struct sensor_parameters *p);


static int parameters_init(struct sensor_parameter_handles *h)
{
	/* min values */
	h->min[0] = param_find("RC1_MIN");
	h->min[1] = param_find("RC2_MIN");
	h->min[2] = param_find("RC3_MIN");
	h->min[3] = param_find("RC4_MIN");
	h->min[4] = param_find("RC5_MIN");
	h->min[5] = param_find("RC6_MIN");
	h->min[6] = param_find("RC7_MIN");
	h->min[7] = param_find("RC8_MIN");

	/* trim values */
	h->trim[0] = param_find("RC1_TRIM");
	h->trim[1] = param_find("RC2_TRIM");
	h->trim[2] = param_find("RC3_TRIM");
	h->trim[3] = param_find("RC4_TRIM");
	h->trim[4] = param_find("RC5_TRIM");
	h->trim[5] = param_find("RC6_TRIM");
	h->trim[6] = param_find("RC7_TRIM");
	h->trim[7] = param_find("RC8_TRIM");

	/* max values */
	h->max[0] = param_find("RC1_MAX");
	h->max[1] = param_find("RC2_MAX");
	h->max[2] = param_find("RC3_MAX");
	h->max[3] = param_find("RC4_MAX");
	h->max[4] = param_find("RC5_MAX");
	h->max[5] = param_find("RC6_MAX");
	h->max[6] = param_find("RC7_MAX");
	h->max[7] = param_find("RC8_MAX");

	/* channel reverse */
	h->rev[0] = param_find("RC1_REV");
	h->rev[1] = param_find("RC2_REV");
	h->rev[2] = param_find("RC3_REV");
	h->rev[3] = param_find("RC4_REV");
	h->rev[4] = param_find("RC5_REV");
	h->rev[5] = param_find("RC6_REV");
	h->rev[6] = param_find("RC7_REV");
	h->rev[7] = param_find("RC8_REV");

	h->rc_type = param_find("RC_TYPE");

	h->rc_map_roll 		= param_find("RC_MAP_ROLL");
	h->rc_map_pitch 	= param_find("RC_MAP_PITCH");
	h->rc_map_yaw 		= param_find("RC_MAP_YAW");
	h->rc_map_throttle 	= param_find("RC_MAP_THROTTLE");
	h->rc_map_mode_sw 	= param_find("RC_MAP_MODE_SW");

	/* gyro offsets */
	h->gyro_offset[0] = param_find("SENSOR_GYRO_XOFF");
	h->gyro_offset[1] = param_find("SENSOR_GYRO_YOFF");
	h->gyro_offset[2] = param_find("SENSOR_GYRO_ZOFF");

	/* accel offsets */
	h->acc_offset[0] = param_find("SENSOR_ACC_XOFF");
	h->acc_offset[1] = param_find("SENSOR_ACC_YOFF");
	h->acc_offset[2] = param_find("SENSOR_ACC_ZOFF");

	/* mag offsets */
	h->mag_offset[0] = param_find("SENSOR_MAG_XOFF");
	h->mag_offset[1] = param_find("SENSOR_MAG_YOFF");
	h->mag_offset[2] = param_find("SENSOR_MAG_ZOFF");

	h->battery_voltage_scaling = param_find("BAT_V_SCALING");

	return OK;
}

static int parameters_update(const struct sensor_parameter_handles *h, struct sensor_parameters *p)
{
	const unsigned int nchans = 8;

	/* min values */
	for (unsigned int i = 0; i < nchans; i++) {
		param_get(h->min[i], &(p->min[i]));
	}

	/* trim values */
	for (unsigned int i = 0; i < nchans; i++) {
		param_get(h->trim[i], &(p->trim[i]));
	}

	/* max values */
	for (unsigned int i = 0; i < nchans; i++) {
		param_get(h->max[i], &(p->max[i]));
	}

	/* channel reverse */
	for (unsigned int i = 0; i < nchans; i++) {
		param_get(h->rev[i], &(p->rev[i]));
	}

	/* remote control type */
	param_get(h->rc_type, &(p->rc_type));

	/* channel mapping */
	param_get(h->rc_map_roll, &(p->rc_map_roll));
	param_get(h->rc_map_pitch, &(p->rc_map_pitch));
	param_get(h->rc_map_yaw, &(p->rc_map_yaw));
	param_get(h->rc_map_throttle, &(p->rc_map_throttle));
	param_get(h->rc_map_mode_sw, &(p->rc_map_mode_sw));

	/* gyro offsets */
	param_get(h->gyro_offset[0], &(p->gyro_offset[0]));
	param_get(h->gyro_offset[1], &(p->gyro_offset[1]));
	param_get(h->gyro_offset[2], &(p->gyro_offset[2]));

	/* accel offsets */
	param_get(h->acc_offset[0], &(p->acc_offset[0]));
	param_get(h->acc_offset[1], &(p->acc_offset[1]));
	param_get(h->acc_offset[2], &(p->acc_offset[2]));

	/* mag offsets */
	param_get(h->mag_offset[0], &(p->mag_offset[0]));
	param_get(h->mag_offset[1], &(p->mag_offset[1]));
	param_get(h->mag_offset[2], &(p->mag_offset[2]));

	/* scaling of ADC ticks to battery voltage */
	param_get(h->battery_voltage_scaling, &(p->battery_voltage_scaling));

	return OK;
}

/**
 * Initialize all sensor drivers.
 *
 * @return 0 on success, != 0 on failure
 */
static int sensors_init(void)
{
	printf("[sensors] Sensor configuration..\n");

	/* open magnetometer */
	fd_magnetometer = open("/dev/mag", O_RDONLY);

	if (fd_magnetometer < 0) {
		fprintf(stderr, "[sensors]   MAG open fail (err #%d): %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
		fflush(stderr);
		/* this sensor is critical, exit on failed init */
		errno = ENOSYS;
		return ERROR;

	} else {
		printf("[sensors]   MAG open ok\n");
	}

	/* open barometer */
	fd_barometer = open("/dev/baro", O_RDONLY);

	if (fd_barometer < 0) {
		fprintf(stderr, "[sensors]   BARO open fail (err #%d): %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
		fflush(stderr);

	} else {
		printf("[sensors]   BARO open ok\n");
	}

	/* open gyro */
	fd_gyro_l3gd20  = open("/dev/l3gd20", O_RDONLY);
	int errno_gyro = (int)*get_errno_ptr();

	if (!(fd_gyro_l3gd20 < 0)) {
		printf("[sensors]   L3GD20 open ok\n");
	}

	/* open gyro */
	fd_gyro = open("/dev/gyro", O_RDONLY);
	errno_gyro = (int)*get_errno_ptr();

	if (!(fd_gyro < 0)) {
		printf("[sensors]   GYRO open ok\n");
	}

	/* open accelerometer, prefer the MPU-6000 */
	fd_accelerometer = open("/dev/accel", O_RDONLY);
	int errno_accelerometer = (int)*get_errno_ptr();

	if (!(fd_accelerometer < 0)) {
		printf("[sensors]   ACCEL open ok\n");
	}

	/* only attempt to use BMA180 if MPU-6000 is not available */
	int errno_bma180 = 0;
	if (fd_accelerometer < 0) {
		fd_bma180 = open("/dev/bma180", O_RDONLY);
		errno_bma180 = (int)*get_errno_ptr();

		if (!(fd_bma180 < 0)) {
			printf("[sensors]   ACCEL (BMA180) open ok\n");
		}
	} else {
		fd_bma180 = -1;
	}

	/* fail if no accelerometer is available */
	if (fd_accelerometer < 0 && fd_bma180 < 0) {
		/* print error message only if both failed, discard message else at all to not confuse users */
		if (fd_accelerometer < 0) {
			fprintf(stderr, "[sensors]   ACCEL: MPU-6000: open fail (err #%d): %s\n", errno_accelerometer, strerror(errno_accelerometer));
			fflush(stderr);
			/* this sensor is redundant with BMA180 */
		}
		
		if (fd_bma180 < 0) {
			fprintf(stderr, "[sensors]   BMA180: open fail (err #%d): %s\n", errno_bma180, strerror(errno_bma180));
			fflush(stderr);
			/* this sensor is redundant with MPU-6000 */
		}

		errno = ENOSYS;
		return ERROR;
	}

	/* fail if no gyro is available */
	if (fd_accelerometer < 0 && fd_bma180 < 0) {
		/* print error message only if both failed, discard message else at all to not confuse users */
		if (fd_accelerometer < 0) {
			fprintf(stderr, "[sensors]   GYRO: MPU-6000: open fail (err #%d): %s\n", errno_accelerometer, strerror(errno_accelerometer));
			fflush(stderr);
			/* this sensor is redundant with BMA180 */
		}
		
		if (fd_gyro < 0) {
			fprintf(stderr, "[sensors]   L3GD20 open fail (err #%d): %s\n", errno_gyro, strerror(errno_gyro));
			fflush(stderr);
			/* this sensor is critical, exit on failed init */
		}

		errno = ENOSYS;
		return ERROR;
	}

	/* open adc */
	fd_adc = open("/dev/adc0", O_RDONLY | O_NONBLOCK);

	if (fd_adc < 0) {
		fprintf(stderr, "[sensors]   ADC: open fail (err #%d): %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
		fflush(stderr);
		/* this sensor is critical, exit on failed init */
		errno = ENOSYS;
		return ERROR;

	} else {
		printf("[sensors]   ADC open ok\n");
	}

	/* configure gyro - if its not available and we got here the MPU-6000 is for sure available */
	if (fd_gyro_l3gd20  > 0) {
		if (ioctl(fd_gyro_l3gd20 , L3GD20_SETRATE, L3GD20_RATE_760HZ_LP_30HZ) || ioctl(fd_gyro_l3gd20 , L3GD20_SETRANGE, L3GD20_RANGE_500DPS)) {
			fprintf(stderr, "[sensors]   L3GD20 configuration (ioctl) fail (err #%d): %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
			fflush(stderr);
			/* this sensor is critical, exit on failed init */
			errno = ENOSYS;
			return ERROR;

		} else {
			printf("[sensors]   L3GD20 configuration ok\n");
		}
	}

	/* XXX Add IOCTL configuration of remaining sensors */

	printf("[sensors] All sensors configured\n");
	return OK;
}

/**
 * Callback function called by high resolution timer.
 *
 * This function signals a pthread condition and wakes up the
 * sensor main loop.
 */
static void sensors_timer_loop(void *arg)
{
	/* Inform the read thread that it is now time to read */
	sensors_timer_loop_counter++;
	/* Do not use global data broadcast because of
	 * use of printf() in call - would be fatal here
	 */
	pthread_cond_broadcast(&sensors_read_ready);
}

int sensors_thread_main(int argc, char *argv[])
{
	/* inform about start */
	printf("[sensors] Initializing..\n");
	fflush(stdout);
	int ret = OK;

	/* start sensor reading */
	if (sensors_init() != OK) {
		fprintf(stderr, "[sensors] ERROR: Failed to initialize all sensors\n");
		/* Clean up */
		close(fd_gyro);
		close(fd_bma180);
		close(fd_magnetometer);
		close(fd_barometer);
		close(fd_adc);

		fprintf(stderr, "[sensors] REBOOTING SYSTEM\n\n");
		fflush(stderr);
		fflush(stdout);
		usleep(100000);

		/* Sensors are critical, immediately reboot system on failure */
		reboot();
		/* Not ever reaching here */

	} else {
		/* flush stdout from init routine */
		fflush(stdout);
	}

	/* initialize parameters */
	struct sensor_parameters rcp;
	struct sensor_parameter_handles rch;
	parameters_init(&rch);
	parameters_update(&rch, &rcp);

	bool gyro_healthy = false;
	bool acc_healthy = false;
	bool magn_healthy = false;
	bool baro_healthy = false;
	bool adc_healthy = false;

	bool hil_enabled = false;		/**< HIL is disabled by default	*/
	bool publishing = false;		/**< the app is not publishing by default, only if HIL is disabled on first run */

	int magcounter = 0;
	int barocounter = 0;
	int adccounter = 0;

	unsigned int mag_fail_count = 0;
	unsigned int mag_success_count = 0;

	unsigned int baro_fail_count = 0;
	unsigned int baro_success_count = 0;

	unsigned int gyro_fail_count = 0;
	unsigned int gyro_success_count = 0;

	unsigned int acc_fail_count = 0;
	unsigned int acc_success_count = 0;

	unsigned int adc_fail_count = 0;
	unsigned int adc_success_count = 0;

	ssize_t	ret_gyro;
	ssize_t	ret_accelerometer;
	ssize_t	ret_magnetometer;
	ssize_t	ret_barometer;
	ssize_t	ret_adc;
	int 	nsamples_adc;

	/* for PX4FMU 1.5 compatibility */
	int16_t buf_accelerometer[3];

	struct gyro_report buf_gyro;
	struct accel_report buf_accel_report;
	struct mag_report buf_magnetometer;
	struct baro_report buf_barometer;

	// bool mag_calibration_enabled = false;

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
	};
	#pragma pack(pop)

	struct adc_msg4_s buf_adc;
	size_t adc_readsize = 1 * sizeof(struct adc_msg4_s);

	float battery_voltage_conversion;
	battery_voltage_conversion = rcp.battery_voltage_scaling;

	if (-1 == (int)battery_voltage_conversion) {
		/* default is conversion factor for the PX4IO / PX4IOAR board, the factor for PX4FMU standalone is different */
		battery_voltage_conversion = 3.3f * 52.0f / 5.0f / 4095.0f;
	}

#ifdef CONFIG_HRT_PPM
	int ppmcounter = 0;
#endif
	/* initialize to 100 to execute immediately */
	int paramcounter = 100;
	int excessive_readout_time_counter = 0;
	int read_loop_counter = 0;

	/* Empty sensor buffers, avoid junk values */
	/* Read first two values of each sensor into void */
	if (fd_bma180 > 0)(void)read(fd_bma180, buf_accelerometer, sizeof(buf_accelerometer));
	if (fd_gyro > 0)(void)read(fd_gyro, &buf_gyro, sizeof(buf_gyro));
	if (fd_accelerometer > 0)(void)read(fd_accelerometer, &buf_accel_report, sizeof(buf_accel_report));
	(void)read(fd_magnetometer, &buf_magnetometer, sizeof(buf_magnetometer));
	if (fd_barometer > 0)(void)read(fd_barometer, &buf_barometer, sizeof(buf_barometer));

	struct sensor_combined_s raw = {
		.timestamp = hrt_absolute_time(),
		.gyro_raw = {buf_gyro.x_raw, buf_gyro.y_raw, buf_gyro.z_raw},
		.gyro_raw_counter = 0,
		.gyro_rad_s = {buf_gyro.x, buf_gyro.y, buf_gyro.z},
		.accelerometer_raw = {buf_accel_report.x_raw, buf_accel_report.y_raw, buf_accel_report.z_raw},
		.accelerometer_raw_counter = 0,
		.accelerometer_m_s2 = {buf_accel_report.x, buf_accel_report.y, buf_accel_report.z},
		.magnetometer_raw = {buf_magnetometer.x_raw, buf_magnetometer.y_raw, buf_magnetometer.z_raw},
		.magnetometer_raw_counter = 0,
		.baro_pres_mbar = buf_barometer.pressure,
		.baro_alt_meter = buf_barometer.altitude,
		.baro_temp_celcius = buf_barometer.temperature,
		.baro_raw_counter = 0,
		.battery_voltage_v = BAT_VOL_INITIAL,
		.adc_voltage_v = {0, 0, 0},
		.battery_voltage_counter = 0,
		.battery_voltage_valid = false,
	};

	/* condition to wait for */
	pthread_mutex_init(&sensors_read_ready_mutex, NULL);
	pthread_cond_init(&sensors_read_ready, NULL);

	/* advertise the sensor_combined topic and make the initial publication */
	sensor_pub = orb_advertise(ORB_ID(sensor_combined), &raw);
	publishing = true;

	/* advertise the manual_control topic */
	struct manual_control_setpoint_s manual_control = { .mode = ROLLPOS_PITCHPOS_YAWRATE_THROTTLE,
						   .roll = 0.0f,
						   .pitch = 0.0f,
						   .yaw = 0.0f,
						   .throttle = 0.0f };

	orb_advert_t manual_control_pub = orb_advertise(ORB_ID(manual_control_setpoint), &manual_control);

	if (manual_control_pub < 0) {
		fprintf(stderr, "[sensors] ERROR: orb_advertise for topic manual_control_setpoint failed.\n");
	}

	/* advertise the rc topic */
	struct rc_channels_s rc;
	memset(&rc, 0, sizeof(rc));
	orb_advert_t rc_pub = orb_advertise(ORB_ID(rc_channels), &rc);

	if (rc_pub < 0) {
		fprintf(stderr, "[sensors] ERROR: orb_advertise for topic rc_channels failed.\n");
	}

	/* subscribe to system status */
	struct vehicle_status_s vstatus;
	memset(&vstatus, 0, sizeof(vstatus));
	int vstatus_sub = orb_subscribe(ORB_ID(vehicle_status));

	printf("[sensors] rate: %u Hz\n", (unsigned int)(1000000 / SENSOR_INTERVAL_MICROSEC));

	struct hrt_call sensors_hrt_call;
	/* Enable high resolution timer callback to unblock main thread, run after 2 ms */
	hrt_call_every(&sensors_hrt_call, 2000, SENSOR_INTERVAL_MICROSEC, &sensors_timer_loop, NULL);

	thread_running = true;

	while (!thread_should_exit) {
		pthread_mutex_lock(&sensors_read_ready_mutex);

		struct timespec time_to_wait = {0, 0};
		/* Wait 2 seconds until timeout */
		time_to_wait.tv_nsec = 0;
		time_to_wait.tv_sec = time(NULL) + 2;

		if (pthread_cond_timedwait(&sensors_read_ready, &sensors_read_ready_mutex, &time_to_wait) == OK) {
			pthread_mutex_unlock(&sensors_read_ready_mutex);

			bool gyro_updated = false;
			bool acc_updated = false;
			bool magn_updated = false;
			bool baro_updated = false;
			bool adc_updated = false;

			/* store the time closest to all measurements */
			uint64_t current_time = hrt_absolute_time();
			raw.timestamp = current_time;

			/* Update at 5 Hz */
			if (paramcounter == ((unsigned int)(1000000 / SENSOR_INTERVAL_MICROSEC)/5)) {
				
				/* Check HIL state */
				orb_copy(ORB_ID(vehicle_status), vstatus_sub, &vstatus);

				/* switching from non-HIL to HIL mode */
				//printf("[sensors] Vehicle mode: %i \t AND: %i, HIL: %i\n", vstatus.mode, vstatus.mode & VEHICLE_MODE_FLAG_HIL_ENABLED, hil_enabled);
				if (vstatus.flag_hil_enabled && !hil_enabled) {
					hil_enabled = true;
					publishing = false;

					int sens_ret = close(sensor_pub);
					if (sens_ret == OK) {
						printf("[sensors] Closing sensor pub OK\n");
					} else {
						printf("[sensors] FAILED Closing sensor pub, result: %i \n", sens_ret);
					}

					/* switching from HIL to non-HIL mode */

				} else if (!publishing && !hil_enabled) {
					/* advertise the topic and make the initial publication */
					sensor_pub = orb_advertise(ORB_ID(sensor_combined), &raw);
					hil_enabled = false;
					publishing = true;
				}

				/* update parameters */
				parameters_update(&rch, &rcp);

				/* Update RC scalings and function mappings */
				rc.chan[0].scaling_factor = (1.0f / ((rcp.max[0] - rcp.min[0]) / 2.0f) * rcp.rev[0]);
				rc.chan[0].mid = rcp.trim[0];

				rc.chan[1].scaling_factor = (1.0f / ((rcp.max[1] - rcp.min[1]) / 2.0f) * rcp.rev[1]);
				rc.chan[1].mid = rcp.trim[1];

				rc.chan[2].scaling_factor = (1.0f / ((rcp.max[2] - rcp.min[2]) / 2.0f) * rcp.rev[2]);
				rc.chan[2].mid = rcp.trim[2];

				rc.chan[3].scaling_factor = (1.0f / ((rcp.max[3] - rcp.min[3]) / 2.0f) * rcp.rev[3]);
				rc.chan[3].mid = rcp.trim[3];

				rc.chan[4].scaling_factor = (1.0f / ((rcp.max[4] - rcp.min[4]) / 2.0f) * rcp.rev[4]);
				rc.chan[4].mid = rcp.trim[4];

				rc.chan[5].scaling_factor = (1.0f / ((rcp.max[5] - rcp.min[5]) / 2.0f) * rcp.rev[5]);
				rc.chan[5].mid = rcp.trim[5];

				rc.chan[6].scaling_factor = (1.0f / ((rcp.max[6] - rcp.min[6]) / 2.0f) * rcp.rev[6]);
				rc.chan[6].mid = rcp.trim[6];

				rc.chan[7].scaling_factor = (1.0f / ((rcp.max[7] - rcp.min[7]) / 2.0f) * rcp.rev[7]);
				rc.chan[7].mid = rcp.trim[7];

				rc.function[0] = rcp.rc_map_throttle - 1;
				rc.function[1] = rcp.rc_map_roll - 1;
				rc.function[2] = rcp.rc_map_pitch - 1;
				rc.function[3] = rcp.rc_map_yaw - 1;
				rc.function[4] = rcp.rc_map_mode_sw - 1;

				paramcounter = 0;
			}
			paramcounter++;

			if (fd_gyro > 0) {
				/* try reading gyro */
				uint64_t start_gyro = hrt_absolute_time();
				ret_gyro = read(fd_gyro, &buf_gyro, sizeof(buf_gyro));
				int gyrotime = hrt_absolute_time() - start_gyro;

				if (gyrotime > 500) printf("GYRO (pure read): %d us\n", gyrotime);

				/* GYROSCOPE */
				if (ret_gyro != sizeof(buf_gyro)) {
					gyro_fail_count++;

					if ((((gyro_fail_count % 500) == 0) || (gyro_fail_count > 20 && gyro_fail_count < 100)) && (int)*get_errno_ptr() != EAGAIN) {
						fprintf(stderr, "[sensors] GYRO ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
					}

					if (gyro_healthy && gyro_fail_count >= GYRO_HEALTH_COUNTER_LIMIT_ERROR) {
						// global_data_send_subsystem_info(&gyro_present_enabled);
						gyro_healthy = false;
						gyro_success_count = 0;
					}

				} else {
					gyro_success_count++;

					if (!gyro_healthy && gyro_success_count >= GYRO_HEALTH_COUNTER_LIMIT_OK) {
						// global_data_send_subsystem_info(&gyro_present_enabled_healthy);
						gyro_healthy = true;
						gyro_fail_count = 0;

					}

					gyro_updated = true;
				}

				gyrotime = hrt_absolute_time() - start_gyro;

				if (gyrotime > 500) printf("GYRO (complete): %d us\n", gyrotime);
			}

			// if (fd_gyro_l3gd20 > 0) {
			// 	/* try reading gyro */
			// 	uint64_t start_gyro = hrt_absolute_time();
			// 	ret_gyro = read(fd_gyro, buf_gyro_l3gd20, sizeof(buf_gyro_l3gd20));
			// 	int gyrotime = hrt_absolute_time() - start_gyro;

			// 	if (gyrotime > 500) printf("L3GD20 GYRO (pure read): %d us\n", gyrotime);

			// 	/* GYROSCOPE */
			// 	if (ret_gyro != sizeof(buf_gyro)) {
			// 		gyro_fail_count++;

			// 		if ((((gyro_fail_count % 20) == 0) || (gyro_fail_count > 20 && gyro_fail_count < 100)) && (int)*get_errno_ptr() != EAGAIN) {
			// 			fprintf(stderr, "[sensors] L3GD20 ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
			// 		}

			// 		if (gyro_healthy && gyro_fail_count >= GYRO_HEALTH_COUNTER_LIMIT_ERROR) {
			// 			// global_data_send_subsystem_info(&gyro_present_enabled);
			// 			gyro_healthy = false;
			// 			gyro_success_count = 0;
			// 		}

			// 	} else {
			// 		gyro_success_count++;

			// 		if (!gyro_healthy && gyro_success_count >= GYRO_HEALTH_COUNTER_LIMIT_OK) {
			// 			// global_data_send_subsystem_info(&gyro_present_enabled_healthy);
			// 			gyro_healthy = true;
			// 			gyro_fail_count = 0;

			// 		}

			// 		gyro_updated = true;
			// 	}

			// 	gyrotime = hrt_absolute_time() - start_gyro;

			// 	if (gyrotime > 500) printf("L3GD20 GYRO (complete): %d us\n", gyrotime);
			// }

			/* read MPU-6000 */
			if (fd_accelerometer > 0) {
				/* try reading acc */
				uint64_t start_acc = hrt_absolute_time();
				ret_accelerometer = read(fd_accelerometer, &buf_accel_report, sizeof(struct accel_report));

				/* ACCELEROMETER */
				if (ret_accelerometer != sizeof(struct accel_report)) {
					acc_fail_count++;

					if ((acc_fail_count % 500) == 0 || (acc_fail_count > 20 && acc_fail_count < 40)) {
						fprintf(stderr, "[sensors] MPU-6000 ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
					}


					if (acc_healthy && acc_fail_count >= ACC_HEALTH_COUNTER_LIMIT_ERROR) {
						// global_data_send_subsystem_info(&acc_present_enabled);
						gyro_healthy = false;
						acc_success_count = 0;
					}

				} else {
					acc_success_count++;

					if (!acc_healthy && acc_success_count >= ACC_HEALTH_COUNTER_LIMIT_OK) {

						// global_data_send_subsystem_info(&acc_present_enabled_healthy);
						acc_healthy = true;
						acc_fail_count = 0;

					}

					acc_updated = true;
				}

				int acctime = hrt_absolute_time() - start_acc;
				if (acctime > 500) printf("ACC: %d us\n", acctime);
			}

			/* read BMA180. If the MPU-6000 is present, the BMA180 file descriptor won't be open */
			if (fd_bma180 > 0) {
				/* try reading acc */
				uint64_t start_acc = hrt_absolute_time();
				ret_accelerometer = read(fd_bma180, buf_accelerometer, 6);

				/* ACCELEROMETER */
				if (ret_accelerometer != 6) {
					acc_fail_count++;

					if ((acc_fail_count % 500) == 0 || (acc_fail_count > 20 && acc_fail_count < 40)) {
						fprintf(stderr, "[sensors] BMA180 ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
					}

					if (acc_healthy && acc_fail_count >= ACC_HEALTH_COUNTER_LIMIT_ERROR) {
						// global_data_send_subsystem_info(&acc_present_enabled);
						gyro_healthy = false;
						acc_success_count = 0;
					}

				} else {
					acc_success_count++;

					if (!acc_healthy && acc_success_count >= ACC_HEALTH_COUNTER_LIMIT_OK) {

						// global_data_send_subsystem_info(&acc_present_enabled_healthy);
						acc_healthy = true;
						acc_fail_count = 0;

					}

					acc_updated = true;
				}

				int acctime = hrt_absolute_time() - start_acc;
				if (acctime > 500) printf("ACC: %d us\n", acctime);
			}

			/* MAGNETOMETER */
			if (magcounter == 210) { /* 120 Hz */
				uint64_t start_mag = hrt_absolute_time();
				// /* start calibration mode if requested */
				// if (!mag_calibration_enabled && vstatus.preflight_mag_calibration) {
				// 	ioctl(fd_magnetometer, HMC5883L_CALIBRATION_ON, 0);
				// 	printf("[sensors] enabling mag calibration mode\n");
				// 	mag_calibration_enabled = true;
				// } else if (mag_calibration_enabled && !vstatus.preflight_mag_calibration) {
				// 	ioctl(fd_magnetometer, HMC5883L_CALIBRATION_OFF, 0);
				// 	printf("[sensors] disabling mag calibration mode\n");
				// 	mag_calibration_enabled = false;
				// }
				*get_errno_ptr() = 0;
				ret_magnetometer = read(fd_magnetometer, &buf_magnetometer, sizeof(buf_magnetometer));
				int errcode_mag = (int) * get_errno_ptr();
				int magtime = hrt_absolute_time() - start_mag;

				if (magtime > 2000) {
					printf("[sensors] WARN: MAG (pure read): %d us\n", magtime);
				}

				if (ret_magnetometer != sizeof(buf_magnetometer)) {
					mag_fail_count++;


					if ((mag_fail_count % 200) == 0 || (mag_fail_count > 20 && mag_fail_count < 40)) {
						fprintf(stderr, "[sensors] MAG ERROR #%d: %s\n", errcode_mag, strerror(errcode_mag));
					}

					if (magn_healthy && mag_fail_count >= MAGN_HEALTH_COUNTER_LIMIT_ERROR) {
						// global_data_send_subsystem_info(&magn_present_enabled);
						magn_healthy = false;
						mag_success_count = 0;
					}

				} else {
					mag_success_count++;

					if (!magn_healthy && mag_success_count >= MAGN_HEALTH_COUNTER_LIMIT_OK) {
						// global_data_send_subsystem_info(&magn_present_enabled_healthy);
						magn_healthy = true;
						mag_fail_count = 0;
					}

					magn_updated = true;
				}

				magtime = hrt_absolute_time() - start_mag;

				if (magtime > 2000 && (read_loop_counter % 5) == 0) {
					printf("[sensors] WARN: MAG (overall time): %d us, code:\n", magtime);
				}

				magcounter = 0;
			}

			magcounter++;

			/* BAROMETER */
			if (barocounter == 200 && (fd_barometer > 0)) { /* 100 Hz */
				uint64_t start_baro = hrt_absolute_time();
				*get_errno_ptr() = 0;
				ret_barometer = read(fd_barometer, &buf_barometer, sizeof(buf_barometer));

				if (ret_barometer != sizeof(buf_barometer)) {
					baro_fail_count++;

					if (((baro_fail_count % 200) == 0 || (baro_fail_count > 20 && baro_fail_count < 40)) && (int)*get_errno_ptr() != EAGAIN) {
						fprintf(stderr, "[sensors] MS5611 ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
					}

					if (baro_healthy && baro_fail_count >= BARO_HEALTH_COUNTER_LIMIT_ERROR) {
						/* switched from healthy to unhealthy */
						baro_healthy = false;
						baro_success_count = 0;
						// global_data_send_subsystem_info(&baro_present_enabled);
					}

				} else {
					baro_success_count++;

					if (!baro_healthy && baro_success_count >= MAGN_HEALTH_COUNTER_LIMIT_OK) {
						/* switched from unhealthy to healthy */
						baro_healthy = true;
						baro_fail_count = 0;
						// global_data_send_subsystem_info(&baro_present_enabled_healthy);
					}

					baro_updated = true;
				}

				barocounter = 0;
				int barotime = hrt_absolute_time() - start_baro;

				if (barotime > 2000 && (read_loop_counter % 5) == 0) printf("[sensors] WARN: BARO %d us\n", barotime);
			}

			barocounter++;

			/* ADC */
			if (adccounter == 5) {
				ret_adc = read(fd_adc, &buf_adc, adc_readsize);
				nsamples_adc = ret_adc / sizeof(struct adc_msg_s);

				if (ret_adc  < 0 || ((int)(nsamples_adc * sizeof(struct adc_msg_s))) != ret_adc) {
					adc_fail_count++;

					if (((adc_fail_count % 20) == 0 || adc_fail_count < 10) && (int)*get_errno_ptr() != EAGAIN) {
						fprintf(stderr, "[sensors] ADC ERROR #%d: %s\n", (int)*get_errno_ptr(), strerror((int)*get_errno_ptr()));
					}

					if (adc_healthy && adc_fail_count >= ADC_HEALTH_COUNTER_LIMIT_ERROR) {
						adc_healthy = false;
						adc_success_count = 0;
					}

				} else {
					adc_success_count++;

					if (!adc_healthy && adc_success_count >= ADC_HEALTH_COUNTER_LIMIT_OK) {
						adc_healthy = true;
						adc_fail_count = 0;
					}

					adc_updated = true;
				}

				adccounter = 0;

			}

			adccounter++;



#ifdef CONFIG_HRT_PPM
			bool ppm_updated = false;

			/* PPM */
			if (ppmcounter == 5) {

				/* require at least two channels
				 * to consider the signal valid
				 * check that decoded measurement is up to date
				 */
				if (ppm_decoded_channels > 1 && (hrt_absolute_time() - ppm_last_valid_decode) < 45000) {
					/* Read out values from HRT */
					for (unsigned int i = 0; i < ppm_decoded_channels; i++) {
						rc.chan[i].raw = ppm_buffer[i];
						/* Set the range to +-, then scale up */
						rc.chan[i].scale = (ppm_buffer[i] - rc.chan[i].mid) * rc.chan[i].scaling_factor * 10000;
						rc.chan[i].scaled = (ppm_buffer[i] - rc.chan[i].mid) * rc.chan[i].scaling_factor;
					}

					rc.chan_count = ppm_decoded_channels;
					rc.timestamp = ppm_last_valid_decode;

					/* publish a few lines of code later if set to true */
					ppm_updated = true;

					/* roll input */
					manual_control.roll = rc.chan[rc.function[ROLL]].scaled;
					if (manual_control.roll < -1.0f) manual_control.roll = -1.0f;
					if (manual_control.roll >  1.0f) manual_control.roll =  1.0f;

					/* pitch input */
					manual_control.pitch = rc.chan[rc.function[PITCH]].scaled;
					if (manual_control.pitch < -1.0f) manual_control.pitch = -1.0f;
					if (manual_control.pitch >  1.0f) manual_control.pitch =  1.0f;

					/* yaw input */
					manual_control.yaw = rc.chan[rc.function[YAW]].scaled;
					if (manual_control.yaw < -1.0f) manual_control.yaw = -1.0f;
					if (manual_control.yaw >  1.0f) manual_control.yaw =  1.0f;
					
					/* throttle input */
					manual_control.throttle = (rc.chan[rc.function[THROTTLE]].scaled+1.0f)/2.0f;
					if (manual_control.throttle < 0.0f) manual_control.throttle = 0.0f;
					if (manual_control.throttle > 1.0f) manual_control.throttle = 1.0f;

					/* mode switch input */
					manual_control.override_mode_switch = rc.chan[rc.function[OVERRIDE]].scaled;
					if (manual_control.override_mode_switch < -1.0f) manual_control.override_mode_switch = -1.0f;
					if (manual_control.override_mode_switch >  1.0f) manual_control.override_mode_switch =  1.0f;

				}
				ppmcounter = 0;
			}

			ppmcounter++;
#endif

			/* Copy values of gyro, acc, magnetometer & barometer */

			/* GYROSCOPE */
			// if (gyro_updated) {
			// 	/* copy sensor readings to global data and transform coordinates into px4fmu board frame */

			// 	raw.gyro_raw[0] = ((buf_gyro[1] == -32768) ? -32768 : buf_gyro[1]); // x of the board is y of the sensor
			// 	/* assign negated value, except for -SHORT_MAX, as it would wrap there */
			// 	raw.gyro_raw[1] = ((buf_gyro[0] == -32768) ? 32767 : -buf_gyro[0]); // y on the board is -x of the sensor
			// 	raw.gyro_raw[2] = ((buf_gyro[2] == -32768) ? -32768 : buf_gyro[2]); // z of the board is z of the sensor

			// 	/* scale measurements */
			// 	// XXX request scaling from driver instead of hardcoding it
			// 	/* scaling calculated as: raw * (1/(32768*(500/180*PI))) */
			// 	raw.gyro_rad_s[0] = (raw.gyro_raw[0] - rcp.gyro_offset[0]) * 0.000266316109f;
			// 	raw.gyro_rad_s[1] = (raw.gyro_raw[1] - rcp.gyro_offset[1]) * 0.000266316109f;
			// 	raw.gyro_rad_s[2] = (raw.gyro_raw[2] - rcp.gyro_offset[2]) * 0.000266316109f;

			// 	raw.gyro_raw_counter++;
			// }

			/* MPU-6000 update */
			if (gyro_updated && fd_accelerometer > 0) {
				/* copy sensor readings to global data and transform coordinates into px4fmu board frame */

				raw.gyro_raw[0] = ((buf_gyro.y_raw == -32768) ? -32768 : buf_gyro.y_raw); // x of the board is y of the sensor
				/* assign negated value, except for -SHORT_MAX, as it would wrap there */
				raw.gyro_raw[1] = ((buf_gyro.x_raw == -32768) ? 32767 : -buf_gyro.x_raw); // y on the board is -x of the sensor
				raw.gyro_raw[2] = ((buf_gyro.z_raw == -32768) ? -32768 : buf_gyro.z_raw); // z of the board is z of the sensor

				/* scale measurements */
				// XXX request scaling from driver instead of hardcoding it
				/* scaling calculated as: raw * (1/(32768*(500/180*PI))) */
				raw.gyro_rad_s[0] = buf_gyro.y;
				raw.gyro_rad_s[1] = buf_gyro.x;
				raw.gyro_rad_s[2] = buf_gyro.z;

				raw.gyro_raw_counter++;
			}

			/* ACCELEROMETER */
			if (acc_updated) {
				/* copy sensor readings to global data and transform coordinates into px4fmu board frame */

				if (fd_accelerometer > 0) {
					/* MPU-6000 values */

					/* scale from 14 bit to m/s2 */
					raw.accelerometer_m_s2[0] = buf_accel_report.x - rcp.acc_offset[0] * buf_accel_report.scaling;
					raw.accelerometer_m_s2[1] = buf_accel_report.y - rcp.acc_offset[1] * buf_accel_report.scaling;
					raw.accelerometer_m_s2[2] = buf_accel_report.z - rcp.acc_offset[2] * buf_accel_report.scaling;

					raw.accelerometer_raw[0] = buf_accel_report.x_raw;
					raw.accelerometer_raw[1] = buf_accel_report.y_raw;
					raw.accelerometer_raw[2] = buf_accel_report.z_raw;

					raw.accelerometer_raw_counter++;
				} else if (fd_bma180 > 0) {

					/* assign negated value, except for -SHORT_MAX, as it would wrap there */
					raw.accelerometer_raw[0] = (buf_accelerometer[1] == -32768) ? 32767 : -buf_accelerometer[1]; // x of the board is -y of the sensor
					raw.accelerometer_raw[1] = (buf_accelerometer[0] == -32768) ? -32767 : buf_accelerometer[0]; // y on the board is x of the sensor
					raw.accelerometer_raw[2] = (buf_accelerometer[2] == -32768) ? -32767 : buf_accelerometer[2]; // z of the board is z of the sensor


					// XXX read range from sensor
					float range_g = 4.0f;
					/* scale from 14 bit to m/s2 */
					raw.accelerometer_m_s2[0] = (((raw.accelerometer_raw[0] - rcp.acc_offset[0]) * range_g) / 8192.0f) / 9.81f;
					raw.accelerometer_m_s2[1] = (((raw.accelerometer_raw[1] - rcp.acc_offset[1]) * range_g) / 8192.0f) / 9.81f;
					raw.accelerometer_m_s2[2] = (((raw.accelerometer_raw[2] - rcp.acc_offset[2]) * range_g) / 8192.0f) / 9.81f;

					raw.accelerometer_raw_counter++;
				}

				/* Use MPU-6000 */
				if (fd_accelerometer > 0) {
					raw.gyro_raw[0] = buf_gyro.x_raw;
					raw.gyro_raw[1] = buf_gyro.y_raw;
					raw.gyro_raw[2] = buf_gyro.z_raw;

					/* scaled measurements */
					raw.gyro_rad_s[0] = (buf_gyro.x - rcp.gyro_offset[0]) * buf_gyro.scaling;
					raw.gyro_rad_s[1] = (buf_gyro.y - rcp.gyro_offset[1]) * buf_gyro.scaling;
					raw.gyro_rad_s[2] = (buf_gyro.z - rcp.gyro_offset[2]) * buf_gyro.scaling;

					raw.gyro_raw_counter++;
					/* mark as updated */
					gyro_updated = true;
				}
			}

			/* MAGNETOMETER */
			if (magn_updated) {
				/* copy sensor readings to global data and transform coordinates into px4fmu board frame */

				/* assign negated value, except for -SHORT_MAX, as it would wrap there */
				raw.magnetometer_raw[0] = buf_magnetometer.x_raw;
				raw.magnetometer_raw[1] = buf_magnetometer.y_raw;
				raw.magnetometer_raw[2] = buf_magnetometer.z_raw;

				// XXX Read out mag range via I2C on init, assuming 0.88 Ga and 12 bit res here
				raw.magnetometer_ga[0] = buf_magnetometer.x - rcp.mag_offset[0] * buf_magnetometer.scaling;
				raw.magnetometer_ga[1] = buf_magnetometer.y - rcp.mag_offset[1] * buf_magnetometer.scaling;
				raw.magnetometer_ga[2] = buf_magnetometer.z - rcp.mag_offset[2] * buf_magnetometer.scaling;

				/* store mode */
				raw.magnetometer_mode = 0;

				raw.magnetometer_raw_counter++;
			}

			/* BAROMETER */
			if (baro_updated) {
				/* copy sensor readings to global data and transform coordinates into px4fmu board frame */

				raw.baro_pres_mbar = buf_barometer.pressure; // Pressure in mbar
				raw.baro_alt_meter = buf_barometer.altitude; // Altitude in meters
				raw.baro_temp_celcius = buf_barometer.temperature; // Temperature in degrees celcius

				raw.baro_raw_counter++;
			}

			/* ADC */
			if (adc_updated) {
				/* copy sensor readings to global data*/

				if (ADC_BATTERY_VOLATGE_CHANNEL == buf_adc.am_channel1) {
					/* Voltage in volts */
					raw.battery_voltage_v = (BAT_VOL_LOWPASS_1 * (raw.battery_voltage_v + BAT_VOL_LOWPASS_2 * (buf_adc.am_data1 * battery_voltage_conversion)));

					if ((buf_adc.am_data1 * battery_voltage_conversion) < VOLTAGE_BATTERY_IGNORE_THRESHOLD_VOLTS) {
						raw.battery_voltage_valid = false;
						raw.battery_voltage_v = 0.f;

					} else {
						raw.battery_voltage_valid = true;
					}

					raw.battery_voltage_counter++;
				}
			}

			uint64_t total_time = hrt_absolute_time() - current_time;

			/* Inform other processes that new data is available to copy */
			if ((gyro_updated || acc_updated || magn_updated || baro_updated) && publishing) {
				/* Values changed, publish */
				orb_publish(ORB_ID(sensor_combined), sensor_pub, &raw);
			}

#ifdef CONFIG_HRT_PPM

			if (ppm_updated) {
				orb_publish(ORB_ID(rc_channels), rc_pub, &rc);
				orb_publish(ORB_ID(manual_control_setpoint), manual_control_pub, &manual_control);
			}

#endif

			if (total_time > 2600) {
				excessive_readout_time_counter++;
			}

			if (total_time > 2600 && excessive_readout_time_counter > 100 && excessive_readout_time_counter % 100 == 0) {
				fprintf(stderr, "[sensors] slow update (>2600 us): %d us (#%d)\n", (int)total_time, excessive_readout_time_counter);

			} else if (total_time > 6000) {
				if (excessive_readout_time_counter < 100 || excessive_readout_time_counter % 100 == 0) fprintf(stderr, "[sensors] WARNING: Slow update (>6000 us): %d us (#%d)\n", (int)total_time, excessive_readout_time_counter);
			}


			read_loop_counter++;
#ifdef CONFIG_SENSORS_DEBUG_ENABLED

			if (read_loop_counter % 1000 == 0) printf("[sensors] read loop counter: %d\n", read_loop_counter);

			fflush(stdout);

			if (sensors_timer_loop_counter % 1000 == 0) printf("[sensors] timer/trigger loop counter: %d\n", sensors_timer_loop_counter);

#endif
		}

		if (thread_should_exit) break;
	}

	/* Never really getting here */
	printf("[sensors] sensor readout stopped\n");

	close(fd_gyro);
	close(fd_bma180);
	close(fd_magnetometer);
	close(fd_barometer);
	close(fd_adc);

	printf("[sensors] exiting.\n");

	thread_running = false;

	return ret;
}

static void
usage(const char *reason)
{
	if (reason)
		fprintf(stderr, "%s\n", reason);
	fprintf(stderr, "usage: sensors {start|stop|status}\n");
	exit(1);
}

int sensors_main(int argc, char *argv[])
{
	if (argc < 1)
		usage("missing command");

	if (!strcmp(argv[1], "start")) {

		if (thread_running) {
			printf("sensors app already running\n");
		} else {
			thread_should_exit = false;
			sensors_task = task_create("sensors", SCHED_PRIORITY_MAX - 5, 4096, sensors_thread_main, (argv) ? (const char **)&argv[2] : (const char **)NULL);
		}
		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		if (!thread_running) {
			printf("sensors app not started\n");
		} else {
			printf("stopping sensors app\n");
			thread_should_exit = true;
		}
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (thread_running) {
			printf("\tsensors app is running\n");
		} else {
			printf("\tsensors app not started\n");
		}
		exit(0);
	}

	usage("unrecognized command");
	exit(1);
}

