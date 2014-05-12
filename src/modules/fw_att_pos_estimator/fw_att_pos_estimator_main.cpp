/****************************************************************************
 *
 *   Copyright (c) 2013, 2014 PX4 Development Team. All rights reserved.
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
 * @file fw_att_pos_estimator_main.cpp
 * Implementation of the attitude and position estimator.
 *
 * @author Paul Riseborough <p_riseborough@live.com.au>
 * @author Lorenz Meier <lm@inf.ethz.ch>
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <time.h>
#include <drivers/drv_hrt.h>

#define SENSOR_COMBINED_SUB


#include <drivers/drv_gyro.h>
#include <drivers/drv_accel.h>
#include <drivers/drv_mag.h>
#include <drivers/drv_baro.h>
#ifdef SENSOR_COMBINED_SUB
#include <uORB/topics/sensor_combined.h>
#endif
#include <arch/board/board.h>
#include <uORB/uORB.h>
#include <uORB/topics/airspeed.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_gps_position.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/estimator_status.h>
#include <uORB/topics/actuator_armed.h>
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <geo/geo.h>
#include <systemlib/perf_counter.h>
#include <systemlib/systemlib.h>
#include <mathlib/mathlib.h>
#include <mavlink/mavlink_log.h>

#include "estimator.h"



/**
 * estimator app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int fw_att_pos_estimator_main(int argc, char *argv[]);

__EXPORT uint32_t millis();

static uint64_t last_run = 0;
static uint64_t IMUmsec = 0;

uint32_t millis()
{
	return IMUmsec;
}

static void print_status();

class FixedwingEstimator
{
public:
	/**
	 * Constructor
	 */
	FixedwingEstimator();

	/**
	 * Destructor, also kills the sensors task.
	 */
	~FixedwingEstimator();

	/**
	 * Start the sensors task.
	 *
	 * @return		OK on success.
	 */
	int		start();

	/**
	 * Print the current status.
	 */
	void		print_status();

	/**
	 * Trip the filter by feeding it NaN values.
	 */
	int		trip_nan();

private:

	bool		_task_should_exit;		/**< if true, sensor task should exit */
	int		_estimator_task;			/**< task handle for sensor task */
#ifndef SENSOR_COMBINED_SUB
	int		_gyro_sub;			/**< gyro sensor subscription */
	int		_accel_sub;			/**< accel sensor subscription */
	int		_mag_sub;			/**< mag sensor subscription */
#else
	int		_sensor_combined_sub;
#endif
	int		_airspeed_sub;			/**< airspeed subscription */
	int		_baro_sub;			/**< barometer subscription */
	int		_gps_sub;			/**< GPS subscription */
	int		_vstatus_sub;			/**< vehicle status subscription */
	int 		_params_sub;			/**< notification of parameter updates */
	int 		_manual_control_sub;		/**< notification of manual control updates */
	int		_mission_sub;

	orb_advert_t	_att_pub;			/**< vehicle attitude */
	orb_advert_t	_global_pos_pub;		/**< global position */
	orb_advert_t	_local_pos_pub;			/**< position in local frame */
	orb_advert_t	_estimator_status_pub;		/**< status of the estimator */

	struct vehicle_attitude_s			_att;			/**< vehicle attitude */
	struct gyro_report				_gyro;
	struct accel_report				_accel;
	struct mag_report				_mag;
	struct airspeed_s				_airspeed;		/**< airspeed */
	struct baro_report				_baro;			/**< baro readings */
	struct vehicle_status_s				_vstatus;		/**< vehicle status */
	struct vehicle_global_position_s		_global_pos;		/**< global vehicle position */
	struct vehicle_local_position_s			_local_pos;		/**< local vehicle position */
	struct vehicle_gps_position_s			_gps;			/**< GPS position */

	struct gyro_scale				_gyro_offsets;
	struct accel_scale				_accel_offsets;
	struct mag_scale				_mag_offsets;

#ifdef SENSOR_COMBINED_SUB
	struct sensor_combined_s			_sensor_combined;
#endif

	struct map_projection_reference_s	_pos_ref;

	float						_baro_ref;		/**< barometer reference altitude */
	float						_baro_gps_offset;	/**< offset between GPS and baro */

	perf_counter_t	_loop_perf;			/**< loop performance counter */
	perf_counter_t	_perf_gyro;			///<local performance counter for gyro updates
	perf_counter_t	_perf_accel;			///<local performance counter for accel updates
	perf_counter_t	_perf_mag;			///<local performance counter for mag updates
	perf_counter_t	_perf_gps;			///<local performance counter for gps updates
	perf_counter_t	_perf_baro;			///<local performance counter for baro updates
	perf_counter_t	_perf_airspeed;			///<local performance counter for airspeed updates

	bool						_initialized;
	bool						_gps_initialized;

	int						_mavlink_fd;

	struct {
		int32_t	vel_delay_ms;
		int32_t	pos_delay_ms;
		int32_t	height_delay_ms;
		int32_t	mag_delay_ms;
		int32_t	tas_delay_ms;
	}		_parameters;			/**< local copies of interesting parameters */

	struct {
		param_t vel_delay_ms;
		param_t	pos_delay_ms;
		param_t	height_delay_ms;
		param_t	mag_delay_ms;
		param_t	tas_delay_ms;
	}		_parameter_handles;		/**< handles for interesting parameters */

	AttPosEKF					*_ekf;

	/**
	 * Update our local parameter cache.
	 */
	int		parameters_update();

	/**
	 * Update control outputs
	 *
	 */
	void		control_update();

	/**
	 * Check for changes in vehicle status.
	 */
	void		vehicle_status_poll();

	/**
	 * Shim for calling task_main from task_create.
	 */
	static void	task_main_trampoline(int argc, char *argv[]);

	/**
	 * Main sensor collection task.
	 */
	void		task_main();
};

namespace estimator
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

FixedwingEstimator	*g_estimator;
}

FixedwingEstimator::FixedwingEstimator() :

	_task_should_exit(false),
	_estimator_task(-1),

/* subscriptions */
#ifndef SENSOR_COMBINED_SUB
	_gyro_sub(-1),
	_accel_sub(-1),
	_mag_sub(-1),
#else
	_sensor_combined_sub(-1),
#endif
	_airspeed_sub(-1),
	_baro_sub(-1),
	_gps_sub(-1),
	_vstatus_sub(-1),
	_params_sub(-1),
	_manual_control_sub(-1),

/* publications */
	_att_pub(-1),
	_global_pos_pub(-1),
	_local_pos_pub(-1),
	_estimator_status_pub(-1),

	_baro_ref(0.0f),
	_baro_gps_offset(0.0f),

/* performance counters */
	_loop_perf(perf_alloc(PC_COUNT, "fw_att_pos_estimator")),
	_perf_gyro(perf_alloc(PC_COUNT, "fw_ekf_gyro_upd")),
	_perf_accel(perf_alloc(PC_COUNT, "fw_ekf_accel_upd")),
	_perf_mag(perf_alloc(PC_COUNT, "fw_ekf_mag_upd")),
	_perf_gps(perf_alloc(PC_COUNT, "fw_ekf_gps_upd")),
	_perf_baro(perf_alloc(PC_COUNT, "fw_ekf_baro_upd")),
	_perf_airspeed(perf_alloc(PC_COUNT, "fw_ekf_aspd_upd")),

/* states */
	_initialized(false),
	_gps_initialized(false),
	_mavlink_fd(-1),
	_ekf(nullptr)
{

	_mavlink_fd = open(MAVLINK_LOG_DEVICE, 0);

	_parameter_handles.vel_delay_ms = param_find("PE_VEL_DELAY_MS");
	_parameter_handles.pos_delay_ms = param_find("PE_POS_DELAY_MS");
	_parameter_handles.height_delay_ms = param_find("PE_HGT_DELAY_MS");
	_parameter_handles.mag_delay_ms = param_find("PE_MAG_DELAY_MS");
	_parameter_handles.tas_delay_ms = param_find("PE_TAS_DELAY_MS");

	/* fetch initial parameter values */
	parameters_update();

	/* get offsets */

	int fd, res;

	fd = open(GYRO_DEVICE_PATH, O_RDONLY);

	if (fd > 0) {
		res = ioctl(fd, GYROIOCGSCALE, (long unsigned int)&_gyro_offsets);
		close(fd);
	}

	fd = open(ACCEL_DEVICE_PATH, O_RDONLY);

	if (fd > 0) {
		res = ioctl(fd, ACCELIOCGSCALE, (long unsigned int)&_accel_offsets);
		close(fd);
	}

	fd = open(MAG_DEVICE_PATH, O_RDONLY);

	if (fd > 0) {
		res = ioctl(fd, MAGIOCGSCALE, (long unsigned int)&_mag_offsets);
		close(fd);
	}
}

FixedwingEstimator::~FixedwingEstimator()
{
	if (_estimator_task != -1) {

		/* task wakes up every 100ms or so at the longest */
		_task_should_exit = true;

		/* wait for a second for the task to quit at our request */
		unsigned i = 0;

		do {
			/* wait 20ms */
			usleep(20000);

			/* if we have given up, kill it */
			if (++i > 50) {
				task_delete(_estimator_task);
				break;
			}
		} while (_estimator_task != -1);
	}

	estimator::g_estimator = nullptr;
}

int
FixedwingEstimator::parameters_update()
{

	param_get(_parameter_handles.vel_delay_ms, &(_parameters.vel_delay_ms));
	param_get(_parameter_handles.pos_delay_ms, &(_parameters.pos_delay_ms));
	param_get(_parameter_handles.height_delay_ms, &(_parameters.height_delay_ms));
	param_get(_parameter_handles.mag_delay_ms, &(_parameters.mag_delay_ms));
	param_get(_parameter_handles.tas_delay_ms, &(_parameters.tas_delay_ms));

	return OK;
}

void
FixedwingEstimator::vehicle_status_poll()
{
	bool vstatus_updated;

	/* Check HIL state if vehicle status has changed */
	orb_check(_vstatus_sub, &vstatus_updated);

	if (vstatus_updated) {

		orb_copy(ORB_ID(vehicle_status), _vstatus_sub, &_vstatus);
	}
}

void
FixedwingEstimator::task_main_trampoline(int argc, char *argv[])
{
	estimator::g_estimator->task_main();
}

float dt = 0.0f; // time lapsed since last covariance prediction

void
FixedwingEstimator::task_main()
{

	_ekf = new AttPosEKF();

	if (!_ekf) {
		errx(1, "failed allocating EKF filter - out of RAM!");
	}

	/*
	 * do subscriptions
	 */
	_baro_sub = orb_subscribe(ORB_ID(sensor_baro));
	_airspeed_sub = orb_subscribe(ORB_ID(airspeed));
	_gps_sub = orb_subscribe(ORB_ID(vehicle_gps_position));
	_vstatus_sub = orb_subscribe(ORB_ID(vehicle_status));
	_params_sub = orb_subscribe(ORB_ID(parameter_update));

	/* rate limit vehicle status updates to 5Hz */
	orb_set_interval(_vstatus_sub, 200);

#ifndef SENSOR_COMBINED_SUB

	_gyro_sub = orb_subscribe(ORB_ID(sensor_gyro));
	_accel_sub = orb_subscribe(ORB_ID(sensor_accel));
	_mag_sub = orb_subscribe(ORB_ID(sensor_mag));

	/* rate limit gyro updates to 50 Hz */
	/* XXX remove this!, BUT increase the data buffer size! */
	orb_set_interval(_gyro_sub, 4);
#else
	_sensor_combined_sub = orb_subscribe(ORB_ID(sensor_combined));
	/* XXX remove this!, BUT increase the data buffer size! */
	orb_set_interval(_sensor_combined_sub, 4);
#endif

	parameters_update();

	/* set initial filter state */
	_ekf->fuseVelData = false;
	_ekf->fusePosData = false;
	_ekf->fuseHgtData = false;
	_ekf->fuseMagData = false;
	_ekf->fuseVtasData = false;
	_ekf->statesInitialised = false;

	/* initialize measurement data */
	_ekf->VtasMeas = 0.0f;
	Vector3f lastAngRate = {0.0f, 0.0f, 0.0f};
	Vector3f lastAccel = {0.0f, 0.0f, -9.81f};
	_ekf->dVelIMU.x = 0.0f;
	_ekf->dVelIMU.y = 0.0f;
	_ekf->dVelIMU.z = 0.0f;
	_ekf->dAngIMU.x = 0.0f;
	_ekf->dAngIMU.y = 0.0f;
	_ekf->dAngIMU.z = 0.0f;

	/* wakeup source(s) */
	struct pollfd fds[2];

	/* Setup of loop */
	fds[0].fd = _params_sub;
	fds[0].events = POLLIN;
#ifndef SENSOR_COMBINED_SUB
	fds[1].fd = _gyro_sub;
	fds[1].events = POLLIN;
#else
	fds[1].fd = _sensor_combined_sub;
	fds[1].events = POLLIN;
#endif

	hrt_abstime start_time = hrt_absolute_time();

	bool newDataGps = false;
	bool newAdsData = false;
	bool newDataMag = false;

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

		/* only update parameters if they changed */
		if (fds[0].revents & POLLIN) {
			/* read from param to clear updated flag */
			struct parameter_update_s update;
			orb_copy(ORB_ID(parameter_update), _params_sub, &update);

			/* update parameters from storage */
			parameters_update();
		}

		/* only run estimator if gyro updated */
		if (fds[1].revents & POLLIN) {

			/* check vehicle status for changes to publication state */
			vehicle_status_poll();

			bool accel_updated;
			bool mag_updated;

			perf_count(_perf_gyro);

			/**
			 *    PART ONE: COLLECT ALL DATA
			 **/

			hrt_abstime last_sensor_timestamp;

			/* load local copies */
#ifndef SENSOR_COMBINED_SUB
			orb_copy(ORB_ID(sensor_gyro), _gyro_sub, &_gyro);


			orb_check(_accel_sub, &accel_updated);

			if (accel_updated) {
				perf_count(_perf_accel);
				orb_copy(ORB_ID(sensor_accel), _accel_sub, &_accel);
			}

			last_sensor_timestamp = _gyro.timestamp;
			_ekf.IMUmsec = _gyro.timestamp / 1e3f;

			float deltaT = (_gyro.timestamp - last_run) / 1e6f;
			last_run = _gyro.timestamp;

			/* guard against too large deltaT's */
			if (deltaT > 1.0f)
				deltaT = 0.01f;


			// Always store data, independent of init status
			/* fill in last data set */
			_ekf->dtIMU = deltaT;

			_ekf->angRate.x = _gyro.x;
			_ekf->angRate.y = _gyro.y;
			_ekf->angRate.z = _gyro.z;

			_ekf->accel.x = _accel.x;
			_ekf->accel.y = _accel.y;
			_ekf->accel.z = _accel.z;

			_ekf->dAngIMU = 0.5f * (angRate + lastAngRate) * dtIMU;
			_ekf->lastAngRate = angRate;
			_ekf->dVelIMU = 0.5f * (accel + lastAccel) * dtIMU;
			_ekf->lastAccel = accel;


#else
			orb_copy(ORB_ID(sensor_combined), _sensor_combined_sub, &_sensor_combined);

			static hrt_abstime last_accel = 0;
			static hrt_abstime last_mag = 0;

			if (last_accel != _sensor_combined.accelerometer_timestamp) {
				accel_updated = true;
			}

			last_accel = _sensor_combined.accelerometer_timestamp;


			// Copy gyro and accel
			last_sensor_timestamp = _sensor_combined.timestamp;
			IMUmsec = _sensor_combined.timestamp / 1e3f;

			float deltaT = (_sensor_combined.timestamp - last_run) / 1e6f;
			last_run = _sensor_combined.timestamp;

			/* guard against too large deltaT's */
			if (deltaT > 1.0f || deltaT < 0.000001f)
				deltaT = 0.01f;

			// Always store data, independent of init status
			/* fill in last data set */
			_ekf->dtIMU = deltaT;

			_ekf->angRate.x = _sensor_combined.gyro_rad_s[0];
			_ekf->angRate.y = _sensor_combined.gyro_rad_s[1];
			_ekf->angRate.z = _sensor_combined.gyro_rad_s[2];

			_ekf->accel.x = _sensor_combined.accelerometer_m_s2[0];
			_ekf->accel.y = _sensor_combined.accelerometer_m_s2[1];
			_ekf->accel.z = _sensor_combined.accelerometer_m_s2[2];

			_ekf->dAngIMU = 0.5f * (_ekf->angRate + lastAngRate) * _ekf->dtIMU;
			lastAngRate = _ekf->angRate;
			_ekf->dVelIMU = 0.5f * (_ekf->accel + lastAccel) * _ekf->dtIMU;
			lastAccel = _ekf->accel;

			if (last_mag != _sensor_combined.magnetometer_timestamp) {
				mag_updated = true;
				newDataMag = true;

			} else {
				newDataMag = false;
			}

			last_mag = _sensor_combined.magnetometer_timestamp;

#endif

			bool airspeed_updated;
			orb_check(_airspeed_sub, &airspeed_updated);

			if (airspeed_updated) {
				orb_copy(ORB_ID(airspeed), _airspeed_sub, &_airspeed);
				perf_count(_perf_airspeed);

				_ekf->VtasMeas = _airspeed.true_airspeed_m_s;
				newAdsData = true;

			} else {
				newAdsData = false;
			}

			bool gps_updated;
			orb_check(_gps_sub, &gps_updated);

			if (gps_updated) {

				uint64_t last_gps = _gps.timestamp_position;

				orb_copy(ORB_ID(vehicle_gps_position), _gps_sub, &_gps);
				perf_count(_perf_gps);

				if (_gps.fix_type < 3) {
					gps_updated = false;
					newDataGps = false;

				} else {

					/* check if we had a GPS outage for a long time */
					if (hrt_elapsed_time(&last_gps) > 5 * 1000 * 1000) {
						_ekf->ResetPosition();
						_ekf->ResetVelocity();
						_ekf->ResetStoredStates();
					}

					/* fuse GPS updates */

					//_gps.timestamp / 1e3;
					_ekf->GPSstatus = _gps.fix_type;
					_ekf->velNED[0] = _gps.vel_n_m_s;
					_ekf->velNED[1] = _gps.vel_e_m_s;
					_ekf->velNED[2] = _gps.vel_d_m_s;

					// warnx("GPS updated: status: %d, vel: %8.4f %8.4f %8.4f", (int)GPSstatus, velNED[0], velNED[1], velNED[2]);

					_ekf->gpsLat = math::radians(_gps.lat / (double)1e7);
					_ekf->gpsLon = math::radians(_gps.lon / (double)1e7) - M_PI;
					_ekf->gpsHgt = _gps.alt / 1e3f;
					newDataGps = true;

				}

			}

			bool baro_updated;
			orb_check(_baro_sub, &baro_updated);

			if (baro_updated) {
				orb_copy(ORB_ID(sensor_baro), _baro_sub, &_baro);

				_ekf->baroHgt = _baro.altitude - _baro_ref;

				// Could use a blend of GPS and baro alt data if desired
				_ekf->hgtMea = 1.0f * _ekf->baroHgt + 0.0f * _ekf->gpsHgt;
			}

#ifndef SENSOR_COMBINED_SUB
			orb_check(_mag_sub, &mag_updated);
#endif

			if (mag_updated) {

				perf_count(_perf_mag);

#ifndef SENSOR_COMBINED_SUB
				orb_copy(ORB_ID(sensor_mag), _mag_sub, &_mag);

				// XXX we compensate the offsets upfront - should be close to zero.
				// 0.001f
				_ekf->magData.x = _mag.x;
				_ekf->magBias.x = 0.000001f; // _mag_offsets.x_offset

				_ekf->magData.y = _mag.y;
				_ekf->magBias.y = 0.000001f; // _mag_offsets.y_offset

				_ekf->magData.z = _mag.z;
				_ekf->magBias.z = 0.000001f; // _mag_offsets.y_offset

#else

				// XXX we compensate the offsets upfront - should be close to zero.
				// 0.001f
				_ekf->magData.x = _sensor_combined.magnetometer_ga[0];
				_ekf->magBias.x = 0.000001f; // _mag_offsets.x_offset

				_ekf->magData.y = _sensor_combined.magnetometer_ga[1];
				_ekf->magBias.y = 0.000001f; // _mag_offsets.y_offset

				_ekf->magData.z = _sensor_combined.magnetometer_ga[2];
				_ekf->magBias.z = 0.000001f; // _mag_offsets.y_offset

#endif

				newDataMag = true;

			} else {
				newDataMag = false;
			}


			/**
			 *    CHECK IF THE INPUT DATA IS SANE
			 */
			int check = _ekf->CheckAndBound();

			switch (check) {
				case 0:
					/* all ok */
					break;
				case 1:
				{
					const char* str = "NaN in states, resetting";
					warnx("%s", str);
					mavlink_log_critical(_mavlink_fd, str);
					break;
				}
				case 2:
				{
					const char* str = "stale IMU data, resetting";
					warnx("%s", str);
					mavlink_log_critical(_mavlink_fd, str);
					break;
				}
				case 3:
				{
					const char* str = "switching dynamic / static state";
					warnx("%s", str);
					mavlink_log_critical(_mavlink_fd, str);
					break;
				}
			}

			// If non-zero, we got a problem
			if (check) {

				struct ekf_status_report ekf_report;

				_ekf->GetLastErrorState(&ekf_report);

				struct estimator_status_report rep;
				memset(&rep, 0, sizeof(rep));
				rep.timestamp = hrt_absolute_time();

				rep.states_nan = ekf_report.statesNaN;
				rep.covariance_nan = ekf_report.covarianceNaN;
				rep.kalman_gain_nan = ekf_report.kalmanGainsNaN;

				// Copy all states or at least all that we can fit
				int i = 0;
				unsigned ekf_n_states = (sizeof(ekf_report.states) / sizeof(ekf_report.states[0]));
				unsigned max_states = (sizeof(rep.states) / sizeof(rep.states[0]));
				rep.n_states = (ekf_n_states < max_states) ? ekf_n_states : max_states;

				while ((i < ekf_n_states) && (i < max_states)) {

					rep.states[i] = ekf_report.states[i];
					i++;
				}

				if (_estimator_status_pub > 0) {
					orb_publish(ORB_ID(estimator_status), _estimator_status_pub, &rep);
				} else {
					_estimator_status_pub = orb_advertise(ORB_ID(estimator_status), &rep);
				}
			}


			/**
			 *    PART TWO: EXECUTE THE FILTER
			 **/

			// Wait long enough to ensure all sensors updated once
			// XXX we rather want to check all updated


			if (hrt_elapsed_time(&start_time) > 100000) {

				if (!_gps_initialized && (_ekf->GPSstatus == 3)) {
					_ekf->velNED[0] = _gps.vel_n_m_s;
					_ekf->velNED[1] = _gps.vel_e_m_s;
					_ekf->velNED[2] = _gps.vel_d_m_s;

					double lat = _gps.lat * 1e-7;
					double lon = _gps.lon * 1e-7;
					float alt = _gps.alt * 1e-3;

					_ekf->InitialiseFilter(_ekf->velNED);

					// Initialize projection
					_local_pos.ref_lat = _gps.lat;
					_local_pos.ref_lon = _gps.lon;
					_local_pos.ref_alt = alt;
					_local_pos.ref_timestamp = _gps.timestamp_position;

					// Store 
					orb_copy(ORB_ID(sensor_baro), _baro_sub, &_baro);
					_baro_ref = _baro.altitude;
					_ekf->baroHgt = _baro.altitude - _baro_ref;
					_baro_gps_offset = _baro_ref - _local_pos.ref_alt;

					// XXX this is not multithreading safe
					map_projection_init(&_pos_ref, lat, lon);
					mavlink_log_info(_mavlink_fd, "[position estimator] init ref: lat=%.7f, lon=%.7f, alt=%.2f", lat, lon, alt);

					_gps_initialized = true;

				} else if (!_ekf->statesInitialised) {
					_ekf->velNED[0] = 0.0f;
					_ekf->velNED[1] = 0.0f;
					_ekf->velNED[2] = 0.0f;
					_ekf->posNED[0] = 0.0f;
					_ekf->posNED[1] = 0.0f;
					_ekf->posNED[2] = 0.0f;

					_ekf->posNE[0] = _ekf->posNED[0];
					_ekf->posNE[1] = _ekf->posNED[1];
					_ekf->InitialiseFilter(_ekf->velNED);
				}
			}

			// If valid IMU data and states initialised, predict states and covariances
			if (_ekf->statesInitialised) {
				// Run the strapdown INS equations every IMU update
				_ekf->UpdateStrapdownEquationsNED();
#if 0
				// debug code - could be tunred into a filter mnitoring/watchdog function
				float tempQuat[4];

				for (uint8_t j = 0; j <= 3; j++) tempQuat[j] = states[j];

				quat2eul(eulerEst, tempQuat);

				for (uint8_t j = 0; j <= 2; j++) eulerDif[j] = eulerEst[j] - ahrsEul[j];

				if (eulerDif[2] > pi) eulerDif[2] -= 2 * pi;

				if (eulerDif[2] < -pi) eulerDif[2] += 2 * pi;

#endif
				// store the predicted states for subsequent use by measurement fusion
				_ekf->StoreStates(IMUmsec);
				// Check if on ground - status is used by covariance prediction
				_ekf->OnGroundCheck();
				// sum delta angles and time used by covariance prediction
				_ekf->summedDelAng = _ekf->summedDelAng + _ekf->correctedDelAng;
				_ekf->summedDelVel = _ekf->summedDelVel + _ekf->dVelIMU;
				dt += _ekf->dtIMU;

				// perform a covariance prediction if the total delta angle has exceeded the limit
				// or the time limit will be exceeded at the next IMU update
				if ((dt >= (covTimeStepMax - _ekf->dtIMU)) || (_ekf->summedDelAng.length() > covDelAngMax)) {
					_ekf->CovariancePrediction(dt);
					_ekf->summedDelAng = _ekf->summedDelAng.zero();
					_ekf->summedDelVel = _ekf->summedDelVel.zero();
					dt = 0.0f;
				}

				_initialized = true;
			}

			// Fuse GPS Measurements
			if (newDataGps && _gps_initialized) {
				// Convert GPS measurements to Pos NE, hgt and Vel NED
				_ekf->velNED[0] = _gps.vel_n_m_s;
				_ekf->velNED[1] = _gps.vel_e_m_s;
				_ekf->velNED[2] = _gps.vel_d_m_s;
				_ekf->calcposNED(_ekf->posNED, _ekf->gpsLat, _ekf->gpsLon, _ekf->gpsHgt, _ekf->latRef, _ekf->lonRef, _ekf->hgtRef);

				_ekf->posNE[0] = _ekf->posNED[0];
				_ekf->posNE[1] = _ekf->posNED[1];
				// set fusion flags
				_ekf->fuseVelData = true;
				_ekf->fusePosData = true;
				// recall states stored at time of measurement after adjusting for delays
				_ekf->RecallStates(_ekf->statesAtVelTime, (IMUmsec - _parameters.vel_delay_ms));
				_ekf->RecallStates(_ekf->statesAtPosTime, (IMUmsec - _parameters.pos_delay_ms));
				// run the fusion step
				_ekf->FuseVelposNED();

			} else if (_ekf->statesInitialised) {
				// Convert GPS measurements to Pos NE, hgt and Vel NED
				_ekf->velNED[0] = 0.0f;
				_ekf->velNED[1] = 0.0f;
				_ekf->velNED[2] = 0.0f;
				_ekf->posNED[0] = 0.0f;
				_ekf->posNED[1] = 0.0f;
				_ekf->posNED[2] = 0.0f;

				_ekf->posNE[0] = _ekf->posNED[0];
				_ekf->posNE[1] = _ekf->posNED[1];
				// set fusion flags
				_ekf->fuseVelData = true;
				_ekf->fusePosData = true;
				// recall states stored at time of measurement after adjusting for delays
				_ekf->RecallStates(_ekf->statesAtVelTime, (IMUmsec - _parameters.vel_delay_ms));
				_ekf->RecallStates(_ekf->statesAtPosTime, (IMUmsec - _parameters.pos_delay_ms));
				// run the fusion step
				_ekf->FuseVelposNED();

			} else {
				_ekf->fuseVelData = false;
				_ekf->fusePosData = false;
			}

			if (newAdsData && _ekf->statesInitialised) {
				// Could use a blend of GPS and baro alt data if desired
				_ekf->hgtMea = 1.0f * _ekf->baroHgt + 0.0f * _ekf->gpsHgt;
				_ekf->fuseHgtData = true;
				// recall states stored at time of measurement after adjusting for delays
				_ekf->RecallStates(_ekf->statesAtHgtTime, (IMUmsec - _parameters.height_delay_ms));
				// run the fusion step
				_ekf->FuseVelposNED();

			} else {
				_ekf->fuseHgtData = false;
			}

			// Fuse Magnetometer Measurements
			if (newDataMag && _ekf->statesInitialised) {
				_ekf->fuseMagData = true;
				_ekf->RecallStates(_ekf->statesAtMagMeasTime, (IMUmsec - _parameters.mag_delay_ms)); // Assume 50 msec avg delay for magnetometer data

			} else {
				_ekf->fuseMagData = false;
			}

			if (_ekf->statesInitialised) _ekf->FuseMagnetometer();

			// Fuse Airspeed Measurements
			if (newAdsData && _ekf->statesInitialised && _ekf->VtasMeas > 8.0f) {
				_ekf->fuseVtasData = true;
				_ekf->RecallStates(_ekf->statesAtVtasMeasTime, (IMUmsec - _parameters.tas_delay_ms)); // assume 100 msec avg delay for airspeed data
				_ekf->FuseAirspeed();

			} else {
				_ekf->fuseVtasData = false;
			}

			// Publish results
			if (_initialized && (check == OK)) {



				// State vector:
				// 0-3: quaternions (q0, q1, q2, q3)
				// 4-6: Velocity - m/sec (North, East, Down)
				// 7-9: Position - m (North, East, Down)
				// 10-12: Delta Angle bias - rad (X,Y,Z)
				// 13-14: Wind Vector  - m/sec (North,East)
				// 15-17: Earth Magnetic Field Vector - milligauss (North, East, Down)
				// 18-20: Body Magnetic Field Vector - milligauss (X,Y,Z)

				math::Quaternion q(_ekf->states[0], _ekf->states[1], _ekf->states[2], _ekf->states[3]);
				math::Matrix<3, 3> R = q.to_dcm();
				math::Vector<3> euler = R.to_euler();

				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
						_att.R[i][j] = R(i, j);

				_att.timestamp = last_sensor_timestamp;
				_att.q[0] = _ekf->states[0];
				_att.q[1] = _ekf->states[1];
				_att.q[2] = _ekf->states[2];
				_att.q[3] = _ekf->states[3];
				_att.q_valid = true;
				_att.R_valid = true;

				_att.timestamp = last_sensor_timestamp;
				_att.roll = euler(0);
				_att.pitch = euler(1);
				_att.yaw = euler(2);

				_att.rollspeed = _ekf->angRate.x - _ekf->states[10];
				_att.pitchspeed = _ekf->angRate.y - _ekf->states[11];
				_att.yawspeed = _ekf->angRate.z - _ekf->states[12];
				// gyro offsets
				_att.rate_offsets[0] = _ekf->states[10];
				_att.rate_offsets[1] = _ekf->states[11];
				_att.rate_offsets[2] = _ekf->states[12];

				/* lazily publish the attitude only once available */
				if (_att_pub > 0) {
					/* publish the attitude setpoint */
					orb_publish(ORB_ID(vehicle_attitude), _att_pub, &_att);

				} else {
					/* advertise and publish */
					_att_pub = orb_advertise(ORB_ID(vehicle_attitude), &_att);
				}
			}

			if (_gps_initialized) {
				_local_pos.timestamp = last_sensor_timestamp;
				_local_pos.x = _ekf->states[7];
				_local_pos.y = _ekf->states[8];
				_local_pos.z = _ekf->states[9];

				_local_pos.vx = _ekf->states[4];
				_local_pos.vy = _ekf->states[5];
				_local_pos.vz = _ekf->states[6];

				_local_pos.xy_valid = _gps_initialized;
				_local_pos.z_valid = true;
				_local_pos.v_xy_valid = _gps_initialized;
				_local_pos.v_z_valid = true;
				_local_pos.xy_global = true;

				_local_pos.z_global = false;
				_local_pos.yaw = _att.yaw;

				/* lazily publish the local position only once available */
				if (_local_pos_pub > 0) {
					/* publish the attitude setpoint */
					orb_publish(ORB_ID(vehicle_local_position), _local_pos_pub, &_local_pos);

				} else {
					/* advertise and publish */
					_local_pos_pub = orb_advertise(ORB_ID(vehicle_local_position), &_local_pos);
				}

				_global_pos.timestamp = _local_pos.timestamp;

				if (_local_pos.xy_global) {
					double est_lat, est_lon;
					map_projection_reproject(&_pos_ref, _local_pos.x, _local_pos.y, &est_lat, &est_lon);
					_global_pos.lat = est_lat;
					_global_pos.lon = est_lon;
					_global_pos.time_gps_usec = _gps.time_gps_usec;
				}

				if (_local_pos.v_xy_valid) {
					_global_pos.vel_n = _local_pos.vx;
					_global_pos.vel_e = _local_pos.vy;
				} else {
					_global_pos.vel_n = 0.0f;
					_global_pos.vel_e = 0.0f;
				}

				/* local pos alt is negative, change sign and add alt offset */
				_global_pos.alt = _local_pos.ref_alt + (-_local_pos.z);

				if (_local_pos.v_z_valid) {
					_global_pos.vel_d = _local_pos.vz;
				}

				_global_pos.yaw = _local_pos.yaw;

				_global_pos.eph = _gps.eph_m;
				_global_pos.epv = _gps.epv_m;

				_global_pos.timestamp = _local_pos.timestamp;

				/* lazily publish the global position only once available */
				if (_global_pos_pub > 0) {
					/* publish the attitude setpoint */
					orb_publish(ORB_ID(vehicle_global_position), _global_pos_pub, &_global_pos);

				} else {
					/* advertise and publish */
					_global_pos_pub = orb_advertise(ORB_ID(vehicle_global_position), &_global_pos);
				}
			}

		}

		perf_end(_loop_perf);
	}

	warnx("exiting.\n");

	_estimator_task = -1;
	_exit(0);
}

int
FixedwingEstimator::start()
{
	ASSERT(_estimator_task == -1);

	/* start the task */
	_estimator_task = task_spawn_cmd("fw_att_pos_estimator",
					 SCHED_DEFAULT,
					 SCHED_PRIORITY_MAX - 40,
					 6000,
					 (main_t)&FixedwingEstimator::task_main_trampoline,
					 nullptr);

	if (_estimator_task < 0) {
		warn("task start failed");
		return -errno;
	}

	return OK;
}

void
FixedwingEstimator::print_status()
{
	math::Quaternion q(_ekf->states[0], _ekf->states[1], _ekf->states[2], _ekf->states[3]);
	math::Matrix<3, 3> R = q.to_dcm();
	math::Vector<3> euler = R.to_euler();

	printf("attitude: roll: %8.4f, pitch %8.4f, yaw: %8.4f degrees\n",
	       (double)math::degrees(euler(0)), (double)math::degrees(euler(1)), (double)math::degrees(euler(2)));

	// State vector:
	// 0-3: quaternions (q0, q1, q2, q3)
	// 4-6: Velocity - m/sec (North, East, Down)
	// 7-9: Position - m (North, East, Down)
	// 10-12: Delta Angle bias - rad (X,Y,Z)
	// 13-14: Wind Vector  - m/sec (North,East)
	// 15-17: Earth Magnetic Field Vector - gauss (North, East, Down)
	// 18-20: Body Magnetic Field Vector - gauss (X,Y,Z)

	printf("dtIMU: %8.6f dt: %8.6f IMUmsec: %d\n", _ekf->dtIMU, dt, (int)IMUmsec);
	printf("dvel: %8.6f %8.6f %8.6f accel: %8.6f %8.6f %8.6f\n", (double)_ekf->dVelIMU.x, (double)_ekf->dVelIMU.y, (double)_ekf->dVelIMU.z, (double)_ekf->accel.x, (double)_ekf->accel.y, (double)_ekf->accel.z);
	printf("dang: %8.4f %8.4f %8.4f dang corr: %8.4f %8.4f %8.4f\n" , (double)_ekf->dAngIMU.x, (double)_ekf->dAngIMU.y, (double)_ekf->dAngIMU.z, (double)_ekf->correctedDelAng.x, (double)_ekf->correctedDelAng.y, (double)_ekf->correctedDelAng.z);
	printf("states (quat)        [1-4]: %8.4f, %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[0], (double)_ekf->states[1], (double)_ekf->states[2], (double)_ekf->states[3]);
	printf("states (vel m/s)     [5-7]: %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[4], (double)_ekf->states[5], (double)_ekf->states[6]);
	printf("states (pos m)      [8-10]: %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[7], (double)_ekf->states[8], (double)_ekf->states[9]);
	printf("states (delta ang) [11-13]: %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[10], (double)_ekf->states[11], (double)_ekf->states[12]);
	printf("states (wind)      [14-15]: %8.4f, %8.4f\n", (double)_ekf->states[13], (double)_ekf->states[14]);
	printf("states (earth mag) [16-18]: %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[15], (double)_ekf->states[16], (double)_ekf->states[17]);
	printf("states (body mag)  [19-21]: %8.4f, %8.4f, %8.4f\n", (double)_ekf->states[18], (double)_ekf->states[19], (double)_ekf->states[20]);
	printf("states: %s %s %s %s %s %s %s %s %s %s\n",
	       (_ekf->statesInitialised) ? "INITIALIZED" : "NON_INIT",
	       (_ekf->onGround) ? "ON_GROUND" : "AIRBORNE",
	       (_ekf->fuseVelData) ? "FUSE_VEL" : "INH_VEL",
	       (_ekf->fusePosData) ? "FUSE_POS" : "INH_POS",
	       (_ekf->fuseHgtData) ? "FUSE_HGT" : "INH_HGT",
	       (_ekf->fuseMagData) ? "FUSE_MAG" : "INH_MAG",
	       (_ekf->fuseVtasData) ? "FUSE_VTAS" : "INH_VTAS",
	       (_ekf->useAirspeed) ? "USE_AIRSPD" : "IGN_AIRSPD",
	       (_ekf->useCompass) ? "USE_COMPASS" : "IGN_COMPASS",
	       (_ekf->staticMode) ? "STATIC_MODE" : "DYNAMIC_MODE");
}

int FixedwingEstimator::trip_nan() {

	int ret = 0;

	// If system is not armed, inject a NaN value into the filter
	int armed_sub = orb_subscribe(ORB_ID(actuator_armed));

	struct actuator_armed_s armed;
	orb_copy(ORB_ID(actuator_armed), armed_sub, &armed);

	if (armed.armed) {
		warnx("ACTUATORS ARMED! NOT TRIPPING SYSTEM");
		ret = 1;
	} else {

		float nan_val = 0.0f / 0.0f;

		warnx("system not armed, tripping state vector with NaN values");
		_ekf->states[5] = nan_val;
		usleep(100000);

		// warnx("tripping covariance #1 with NaN values");
		// KH[2][2] = nan_val; //  intermediate result used for covariance updates
		// usleep(100000);

		// warnx("tripping covariance #2 with NaN values");
		// KHP[5][5] = nan_val; // intermediate result used for covariance updates
		// usleep(100000);

		warnx("tripping covariance #3 with NaN values");
		_ekf->P[3][3] = nan_val; // covariance matrix
		usleep(100000);

		warnx("tripping Kalman gains with NaN values");
		_ekf->Kfusion[0] = nan_val; // Kalman gains
		usleep(100000);

		warnx("tripping stored states[0] with NaN values");
		_ekf->storedStates[0][0] = nan_val;
		usleep(100000);

		warnx("\nDONE - FILTER STATE:");
		print_status();
	}

	close(armed_sub);
	return ret;
}

int fw_att_pos_estimator_main(int argc, char *argv[])
{
	if (argc < 1)
		errx(1, "usage: fw_att_pos_estimator {start|stop|status}");

	if (!strcmp(argv[1], "start")) {

		if (estimator::g_estimator != nullptr)
			errx(1, "already running");

		estimator::g_estimator = new FixedwingEstimator;

		if (estimator::g_estimator == nullptr)
			errx(1, "alloc failed");

		if (OK != estimator::g_estimator->start()) {
			delete estimator::g_estimator;
			estimator::g_estimator = nullptr;
			err(1, "start failed");
		}

		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		if (estimator::g_estimator == nullptr)
			errx(1, "not running");

		delete estimator::g_estimator;
		estimator::g_estimator = nullptr;
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (estimator::g_estimator) {
			warnx("running");

			estimator::g_estimator->print_status();

			exit(0);

		} else {
			errx(1, "not running");
		}
	}

	if (!strcmp(argv[1], "trip")) {
		if (estimator::g_estimator) {
			int ret = estimator::g_estimator->trip_nan();

			exit(ret);

		} else {
			errx(1, "not running");
		}
	}

	warnx("unrecognized command");
	return 1;
}
