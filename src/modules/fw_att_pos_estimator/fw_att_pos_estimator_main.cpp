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
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <geo/geo.h>
#include <systemlib/perf_counter.h>
#include <systemlib/systemlib.h>
#include <mathlib/mathlib.h>

#include "../../../InertialNav/code/estimator.h"



/**
 * estimator app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int fw_att_pos_estimator_main(int argc, char *argv[]);

__EXPORT uint32_t millis();

static uint64_t last_run = 0;
static uint32_t IMUmsec = 0;

uint32_t millis()
{
	return IMUmsec;
}

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

	perf_counter_t	_loop_perf;			/**< loop performance counter */
	perf_counter_t	_perf_gyro;			///<local performance counter for gyro updates
	perf_counter_t	_perf_accel;			///<local performance counter for accel updates
	perf_counter_t	_perf_mag;			///<local performance counter for mag updates
	perf_counter_t	_perf_gps;			///<local performance counter for gps updates
	perf_counter_t	_perf_baro;			///<local performance counter for baro updates
	perf_counter_t	_perf_airspeed;			///<local performance counter for airspeed updates

	bool						_initialized;

	struct {
		float		throttle_cruise;
		uint32_t	vel_delay_ms;
		uint32_t	pos_delay_ms;
		uint32_t	height_delay_ms;
		uint32_t	mag_delay_ms;
		uint32_t	tas_delay_ms;
	}		_parameters;			/**< local copies of interesting parameters */

	struct {
		param_t throttle_cruise;

	}		_parameter_handles;		/**< handles for interesting parameters */


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
	void		task_main() __attribute__((noreturn));
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

/* performance counters */
	_loop_perf(perf_alloc(PC_ELAPSED, "fw_att_pos_estimator")),
	_perf_gyro(perf_alloc(PC_ELAPSED, "fw_ekf_gyro_upd")),
	_perf_accel(perf_alloc(PC_ELAPSED, "fw_ekf_accel_upd")),
	_perf_mag(perf_alloc(PC_ELAPSED, "fw_ekf_mag_upd")),
	_perf_gps(perf_alloc(PC_ELAPSED, "fw_ekf_gps_upd")),
	_perf_baro(perf_alloc(PC_ELAPSED, "fw_ekf_baro_upd")),
	_perf_airspeed(perf_alloc(PC_ELAPSED, "fw_ekf_aspd_upd")),

/* states */
	_initialized(false)
{

	_parameter_handles.throttle_cruise = param_find("NAV_DUMMY");

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

	// XXX NEED TO GET HANDLES FIRST! NEEDS PARAM INIT
	//param_get(_parameter_handles.throttle_cruise, &(_parameters.throttle_cruise));

	_parameters.vel_delay_ms = 230;
	_parameters.pos_delay_ms = 210;
	_parameters.height_delay_ms = 350;
	_parameters.mag_delay_ms = 30;
	_parameters.tas_delay_ms = 210;

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

void
FixedwingEstimator::task_main()
{

	/* inform about start */
	warnx("Initializing..");
	fflush(stdout);

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
	orb_set_interval(_gyro_sub, 17);
	#else
	_sensor_combined_sub = orb_subscribe(ORB_ID(sensor_combined));
	/* XXX remove this!, BUT increase the data buffer size! */
	orb_set_interval(_sensor_combined_sub, 17);
	#endif

	parameters_update();

	Vector3f lastAngRate;
	Vector3f lastAccel;
	/* set initial filter state */
	fuseVelData = false;
	fusePosData = false;
	fuseHgtData = false;
	fuseMagData = false;
	fuseVtasData = false;

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

			/* load local copies */
			#ifndef SENSOR_COMBINED_SUB
			orb_copy(ORB_ID(sensor_gyro), _gyro_sub, &_gyro);
			

			orb_check(_accel_sub, &accel_updated);

			if (accel_updated) {
				perf_count(_perf_accel);
				orb_copy(ORB_ID(sensor_accel), _accel_sub, &_accel);
			}


			IMUmsec = _gyro.timestamp / 1e3f;

			float deltaT = (_gyro.timestamp - last_run) / 1e6f;
			last_run = _gyro.timestamp;

			/* guard against too large deltaT's */
			if (deltaT > 1.0f)
				deltaT = 0.01f;


			// Always store data, independent of init status
			/* fill in last data set */
			dtIMU = deltaT;

			angRate.x = _gyro.x;
			angRate.y = _gyro.y;
			angRate.z = _gyro.z;

			accel.x = _accel.x;
			accel.y = _accel.y;
			accel.z = _accel.z;

			dAngIMU = 0.5f * (angRate + lastAngRate) * dtIMU;
			lastAngRate = angRate;
			dVelIMU = 0.5f * (accel + lastAccel) * dtIMU;
			lastAccel = accel;


			#else
			orb_copy(ORB_ID(sensor_combined), _sensor_combined_sub, &_sensor_combined);

			static hrt_abstime last_accel = 0;
			static hrt_abstime last_mag = 0;

			if (last_accel != _sensor_combined.accelerometer_timestamp) {
				accel_updated = true;
			}
			last_accel = _sensor_combined.accelerometer_timestamp;


			// Copy gyro and accel

			IMUmsec = _sensor_combined.timestamp / 1e3f;

			float deltaT = (_sensor_combined.timestamp - last_run) / 1e6f;
			last_run = _sensor_combined.timestamp;

			/* guard against too large deltaT's */
			if (deltaT > 1.0f)
				deltaT = 0.01f;

			// Always store data, independent of init status
			/* fill in last data set */
			dtIMU = deltaT;

			angRate.x = _sensor_combined.gyro_rad_s[0];
			angRate.y = _sensor_combined.gyro_rad_s[1];
			angRate.z = _sensor_combined.gyro_rad_s[2];

			accel.x = _sensor_combined.accelerometer_m_s2[0];
			accel.y = _sensor_combined.accelerometer_m_s2[1];
			accel.z = _sensor_combined.accelerometer_m_s2[2];

			dAngIMU = 0.5f * (angRate + lastAngRate) * dtIMU;
			lastAngRate = angRate;
			dVelIMU = 0.5f * (accel + lastAccel) * dtIMU;
			lastAccel = accel;

			if (last_mag != _sensor_combined.magnetometer_timestamp) {
				mag_updated = true;
			}
			last_mag = _sensor_combined.magnetometer_timestamp;

			#endif

			if (_initialized) {

				/* predict states and covariances */

				/* run the strapdown INS every sensor update */
				UpdateStrapdownEquationsNED();

				/* store the predictions */
				StoreStates();

				/* evaluate if on ground */
				OnGroundCheck();

				/* prepare the delta angles and time used by the covariance prediction */
				summedDelAng = summedDelAng + correctedDelAng;
				summedDelVel = summedDelVel + correctedDelVel;
				dt += dtIMU;

				/* predict the covairance if the total delta angle has exceeded the threshold
				 * or the time limit will be exceeded on the next measurement update
				 */
				if ((dt >= (covTimeStepMax - dtIMU)) || (summedDelAng.length() > covDelAngMax)) {
					CovariancePrediction();
					summedDelAng = summedDelAng.zero();
					summedDelVel = summedDelVel.zero();
					dt = 0.0f;
				}
			}

			bool baro_updated;
			orb_check(_baro_sub, &baro_updated);
			if (baro_updated) {
				orb_copy(ORB_ID(sensor_baro), _baro_sub, &_baro);

				/* XXX leverage baro */
			}

			bool gps_updated;
			orb_check(_gps_sub, &gps_updated);
			if (gps_updated) {
				orb_copy(ORB_ID(vehicle_gps_position), _gps_sub, &_gps);
				perf_count(_perf_gps);

				if (_gps.fix_type > 2) {
					/* fuse GPS updates */

					//_gps.timestamp / 1e3;
					GPSstatus = _gps.fix_type;
					gpsCourse = _gps.cog_rad;
					gpsGndSpd = sqrtf(_gps.vel_n_m_s * _gps.vel_n_m_s + _gps.vel_e_m_s * _gps.vel_e_m_s);
					gpsVelD = _gps.vel_d_m_s;
					gpsLat = math::radians(_gps.lat / (double)1e7);
					gpsLon = math::radians(_gps.lon / (double)1e7);
					gpsHgt = _gps.alt / 1e3f;

					if (hrt_elapsed_time(&start_time) > 500000 && !_initialized) {
						InitialiseFilter();
						_initialized = true;
						warnx("init done.");
						continue;
					}

					if (_initialized) {

						/* convert GPS measurements to horizontal NE, altitude and 3D velocity NED */
						calcvelNED(velNED, gpsCourse, gpsGndSpd, gpsVelD);
						calcposNED(posNED, gpsLat, gpsLon, gpsHgt, latRef, lonRef, hgtRef);

						posNE[0] = posNED[0];
						posNE[1] = posNED[1];
						hgtMea =  -posNED[2];

						// set flags for further processing 
						fuseVelData = true;
						fusePosData = true;
						fuseHgtData = true;

						/* recall states after adjusting for delays */
						RecallStates(statesAtVelTime, (IMUmsec - _parameters.vel_delay_ms));
						RecallStates(statesAtPosTime, (IMUmsec - _parameters.pos_delay_ms));
						RecallStates(statesAtHgtTime, (IMUmsec - _parameters.height_delay_ms));

						/* run the actual fusion */
						FuseVelposNED();
					}

				} else {

					/* do not fuse anything, we got no position / vel update */
					fuseVelData = false;
					fusePosData = false;
					fuseHgtData = false;
				}

			} else {
				/* do not fuse anything, we got no position / vel update */
				fuseVelData = false;
				fusePosData = false;
				fuseHgtData = false;
			}

			#ifndef SENSOR_COMBINED_SUB
			orb_check(_mag_sub, &mag_updated);
			#endif
			if (mag_updated) {

				perf_count(_perf_mag);

				#ifndef SENSOR_COMBINED_SUB
				orb_copy(ORB_ID(sensor_mag), _mag_sub, &_mag);

				// XXX we compensate the offsets upfront - should be close to zero.
				// XXX the purpose of the 0.001 factor is unclear
				// 0.001f
				magData.x = 0.001f * _mag.x;
				magBias.x = 0.0f; // _mag_offsets.x_offset

				magData.y = 0.001f * _mag.y;
				magBias.y = 0.0f; // _mag_offsets.y_offset

				magData.z = 0.001f * _mag.z;
				magBias.z = 0.0f; // _mag_offsets.y_offset
				
				#else

				// XXX we compensate the offsets upfront - should be close to zero.
				// XXX the purpose of the 0.001 factor is unclear
				// 0.001f
				magData.x = 0.001f * _sensor_combined.magnetometer_ga[0];
				magBias.x = 0.0f; // _mag_offsets.x_offset

				magData.y = 0.001f * _sensor_combined.magnetometer_ga[1];
				magBias.y = 0.0f; // _mag_offsets.y_offset

				magData.z = 0.001f * _sensor_combined.magnetometer_ga[2];
				magBias.z = 0.0f; // _mag_offsets.y_offset

				#endif

				if (_initialized) {

					fuseMagData = true;
					RecallStates(statesAtMagMeasTime, (IMUmsec - _parameters.mag_delay_ms));
					FuseMagnetometer();
				}

			} else {
				fuseMagData = false;
			}

			bool airspeed_updated;
			orb_check(_airspeed_sub, &airspeed_updated);
			if (airspeed_updated && _initialized) {
				orb_copy(ORB_ID(airspeed), _airspeed_sub, &_airspeed);
				perf_count(_perf_airspeed);

				if (_airspeed.true_airspeed_m_s > 8.0f /* XXX magic number */) {

					VtasMeas = _airspeed.true_airspeed_m_s;

					if (_initialized) {

						fuseVtasData = true;
						RecallStates(statesAtVtasMeasTime, (IMUmsec - _parameters.tas_delay_ms)); // assume 100 msec avg delay for airspeed data
						FuseAirspeed();
					}
				} else {
					fuseVtasData = false;
				}

			} else {
				fuseVtasData = false;
			}

			if (_initialized) {

				// State vector:
				// 0-3: quaternions (q0, q1, q2, q3)
				// 4-6: Velocity - m/sec (North, East, Down)
				// 7-9: Position - m (North, East, Down)
				// 10-12: Delta Angle bias - rad (X,Y,Z)
				// 13-14: Wind Vector  - m/sec (North,East)
				// 15-17: Earth Magnetic Field Vector - milligauss (North, East, Down)
				// 18-20: Body Magnetic Field Vector - milligauss (X,Y,Z)

				math::Quaternion q(states[0], states[1], states[2], states[3]);
				math::Dcm R(q);
				math::EulerAngles euler(R);

				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
					_att.R[i][j] = R(i, j);

				_att.timestamp = _gyro.timestamp;
				_att.q[0] = states[0];
				_att.q[1] = states[1];
				_att.q[2] = states[2];
				_att.q[3] = states[3];
				_att.q_valid = true;
				_att.R_valid = true;

				_att.timestamp = _gyro.timestamp;
				_att.roll = euler.getPhi();
				_att.pitch = euler.getTheta();
				_att.yaw = euler.getPsi();
				// XXX find the right state
				_att.rollspeed = _gyro.x - states[11];
				_att.pitchspeed = _gyro.y - states[12];
				_att.yawspeed = _gyro.z - states[13];
				// gyro offsets
				_att.rate_offsets[0] = states[11];
				_att.rate_offsets[1] = states[12];
				_att.rate_offsets[2] = states[13];

				/* lazily publish the attitude only once available */
				if (_att_pub > 0) {
					/* publish the attitude setpoint */
					orb_publish(ORB_ID(vehicle_attitude), _att_pub, &_att);

				} else {
					/* advertise and publish */
					_att_pub = orb_advertise(ORB_ID(vehicle_attitude), &_att);
				}

				_local_pos.timestamp = _gyro.timestamp;
				_local_pos.x = states[7];
				_local_pos.y = states[8];
				_local_pos.z = states[9];

				_local_pos.vx = states[4];
				_local_pos.vy = states[5];
				_local_pos.vz = states[6];

				_local_pos.xy_valid = true;
				_local_pos.z_valid = true;
				_local_pos.v_xy_valid = true;
				_local_pos.v_z_valid = true;

				/* lazily publish the local position only once available */
				if (_local_pos_pub > 0) {
					/* publish the attitude setpoint */
					orb_publish(ORB_ID(vehicle_local_position), _local_pos_pub, &_local_pos);

				} else {
					/* advertise and publish */
					_local_pos_pub = orb_advertise(ORB_ID(vehicle_local_position), &_local_pos);
				}

				_global_pos.timestamp = _gyro.timestamp;

				// /* lazily publish the global position only once available */
				// if (_global_pos_pub > 0) {
				// 	/* publish the attitude setpoint */
				// 	orb_publish(ORB_ID(vehicle_global_position), _global_pos_pub, &_global_pos);

				// } else {
				// 	/* advertise and publish */
				// 	_global_pos_pub = orb_advertise(ORB_ID(vehicle_global_position), &_global_pos);
				// }
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

			math::Quaternion q(states[0], states[1], states[2], states[3]);
			math::EulerAngles euler(q);

			printf("attitude: roll: %8.4f, pitch %8.4f, yaw: %8.4f degrees\n",
				(double)math::degrees(euler.getPhi()), (double)math::degrees(euler.getTheta()), (double)math::degrees(euler.getPsi()));

			printf("states   [1-4]: %8.4f, %8.4f, %8.4f, %8.4f\n", (double)states[0], (double)states[1], (double)states[2], (double)states[3]);
			printf("states   [5-7]: %8.4f, %8.4f, %8.4f\n", (double)states[4], (double)states[5], (double)states[6]);
			printf("states  [8-10]: %8.4f, %8.4f, %8.4f\n", (double)states[7], (double)states[8], (double)states[9]);
			printf("states [11-13]: %8.4f, %8.4f, %8.4f\n", (double)states[10], (double)states[11], (double)states[12]);
			printf("states [14-16]: %8.4f, %8.4f, %8.4f\n", (double)states[13], (double)states[14], (double)states[15]);
			printf("states [17-19]: %8.4f, %8.4f, %8.4f\n", (double)states[16], (double)states[17], (double)states[18]);
			printf("states [20-21]: %8.4f, %8.4f\n", (double)states[19], (double)states[20]);
			exit(0);

		} else {
			errx(1, "not running");
		}
	}

	warnx("unrecognized command");
	return 1;
}
