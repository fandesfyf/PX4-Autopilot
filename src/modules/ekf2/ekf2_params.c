/****************************************************************************
 *
 *   Copyright (c) 2015-2016 Estimation and Control Library (ECL). All rights reserved.
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
 * 3. Neither the name ECL nor the names of its contributors may be
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
 * @file parameters.c
 * Parameter definition for ekf2.
 *
 * @author Roman Bast <bapstroman@gmail.com>
 *
 */


/**
 * Minimum time of arrival delta between non-IMU observations before data is downsampled.
 * Baro and Magnetometer data will be averaged before downsampling, other data will be point sampled resulting in loss of information.
 *
 * @group EKF2
 * @min 10
 * @max 50
 * @unit ms
 */
PARAM_DEFINE_INT32(EKF2_MIN_OBS_DT, 20);

/**
 * Magnetometer measurement delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_DELAY, 0);

/**
 * Barometer measurement delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_BARO_DELAY, 0);

/**
 * GPS measurement delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_DELAY, 200);

/**
 * Optical flow measurement delay relative to IMU measurements
 * Assumes measurement is timestamped at trailing edge of integration period
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_OF_DELAY, 5);

/**
 * Range finder measurement delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_DELAY, 5);

/**
 * Airspeed measurement delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_ASP_DELAY, 200);

/**
 * Vision Position Estimator delay relative to IMU measurements
 *
 * @group EKF2
 * @min 0
 * @max 300
 * @unit ms
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_EV_DELAY, 175);

/**
 * Integer bitmask controlling GPS checks.
 *
 * Set bits to 1 to enable checks. Checks enabled by the following bit positions
 * 0 : Minimum required sat count set by EKF2_REQ_NSATS
 * 1 : Minimum required GDoP set by EKF2_REQ_GDOP
 * 2 : Maximum allowed horizontal position error set by EKF2_REQ_EPH
 * 3 : Maximum allowed vertical position error set by EKF2_REQ_EPV
 * 4 : Maximum allowed speed error set by EKF2_REQ_SACC
 * 5 : Maximum allowed horizontal position rate set by EKF2_REQ_HDRIFT. This check can only be used if the vehicle is stationary during alignment.
 * 6 : Maximum allowed vertical position rate set by EKF2_REQ_VDRIFT. This check can only be used if the vehicle is stationary during alignment.
 * 7 : Maximum allowed horizontal speed set by EKF2_REQ_HDRIFT. This check can only be used if the vehicle is stationary during alignment.
 * 8 : Maximum allowed vertical velocity discrepancy set by EKF2_REQ_VDRIFT
 *
 * @group EKF2
 * @min 0
 * @max 511
 * @bit 0 Min sat count (EKF2_REQ_NSATS)
 * @bit 1 Min GDoP (EKF2_REQ_GDOP)
 * @bit 2 Max horizontal position error (EKF2_REQ_EPH)
 * @bit 3 Max vertical position error (EKF2_REQ_EPV)
 * @bit 4 Max speed error (EKF2_REQ_SACC)
 * @bit 5 Max horizontal position rate (EKF2_REQ_HDRIFT)
 * @bit 6 Max vertical position rate (EKF2_REQ_VDRIFT)
 * @bit 7 Max horizontal speed (EKF2_REQ_HDRIFT)
 * @bit 8 Max vertical velocity discrepancy (EKF2_REQ_VDRIFT)
 */
PARAM_DEFINE_INT32(EKF2_GPS_CHECK, 21);

/**
 * Required EPH to use GPS.
 *
 * @group EKF2
 * @min 2
 * @max 100
 * @unit m
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_EPH, 5.0f);

/**
 * Required EPV to use GPS.
 *
 * @group EKF2
 * @min 2
 * @max 100
 * @unit m
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_EPV, 8.0f);

/**
 * Required speed accuracy to use GPS.
 *
 * @group EKF2
 * @min 0.5
 * @max 5.0
 * @unit m/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_SACC, 1.0f);

/**
 * Required satellite count to use GPS.
 *
 * @group EKF2
 * @min 4
 * @max 12
 */
PARAM_DEFINE_INT32(EKF2_REQ_NSATS, 6);

/**
 * Required GDoP to use GPS.
 *
 * @group EKF2
 * @min 1.5
 * @max 5.0
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_GDOP, 2.5f);

/**
 * Maximum horizontal drift speed to use GPS.
 *
 * @group EKF2
 * @min 0.1
 * @max 1.0
 * @unit m/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_HDRIFT, 0.3f);

/**
 * Maximum vertical drift speed to use GPS.
 *
 * @group EKF2
 * @min 0.1
 * @max 1.5
 * @decimal 2
 * @unit m/s
 */
PARAM_DEFINE_FLOAT(EKF2_REQ_VDRIFT, 0.5f);

/**
 * Rate gyro noise for covariance prediction.
 *
 * @group EKF2
 * @min 0.0001
 * @max 0.1
 * @unit rad/s
 * @decimal 4
 */
PARAM_DEFINE_FLOAT(EKF2_GYR_NOISE, 1.5e-2f);

/**
 * Accelerometer noise for covariance prediction.
 *
 * @group EKF2
 * @min 0.01
 * @max 1.0
 * @unit m/s/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_ACC_NOISE, 3.5e-1f);

/**
 * Process noise for IMU rate gyro bias prediction.
 *
 * @group EKF2
 * @min 0.0
 * @max 0.01
 * @unit rad/s**2
 * @decimal 6
 */
PARAM_DEFINE_FLOAT(EKF2_GYR_B_NOISE, 1.0e-3f);

/**
 * Process noise for IMU accelerometer bias prediction.
 *
 * @group EKF2
 * @min 0.0
 * @max 0.01
 * @unit m/s**3
 * @decimal 6
 */
PARAM_DEFINE_FLOAT(EKF2_ACC_B_NOISE, 3.0e-3f);

/**
 * Process noise for body magnetic field prediction.
 *
 * @group EKF2
 * @min 0.0
 * @max 0.1
 * @unit Gauss/s
 * @decimal 6
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_B_NOISE, 1.0e-4f);

/**
 * Process noise for earth magnetic field prediction.
 *
 * @group EKF2
 * @min 0.0
 * @max 0.1
 * @unit Gauss/s
 * @decimal 6
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_E_NOISE, 1.0e-3f);

/**
 * Process noise for wind velocity prediction.
 *
 * @group EKF2
 * @min 0.0
 * @max 1.0
 * @unit m/s/s
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_WIND_NOISE, 1.0e-1f);

/**
 * Measurement noise for gps horizontal velocity.
 *
 * @group EKF2
 * @min 0.01
 * @max 5.0
 * @unit m/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_V_NOISE, 0.5f);

/**
 * Measurement noise for gps position.
 *
 * @group EKF2
 * @min 0.01
 * @max 10.0
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_P_NOISE, 0.5f);

/**
 * Measurement noise for non-aiding position hold.
 *
 * @group EKF2
 * @min 0.5
 * @max 50.0
 * @unit m
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_NOAID_NOISE, 10.0f);

/**
 * Measurement noise for barometric altitude.
 *
 * @group EKF2
 * @min 0.01
 * @max 15.0
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_BARO_NOISE, 2.0f);

/**
 * Measurement noise for magnetic heading fusion.
 *
 * @group EKF2
 * @min 0.01
 * @max 1.0
 * @unit rad
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_HEAD_NOISE, 0.3f);

/**
 * Measurement noise for magnetometer 3-axis fusion.
 *
 * @group EKF2
 * @min 0.001
 * @max 1.0
 * @unit Gauss
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_NOISE, 5.0e-2f);

/**
 * Measurement noise for airspeed fusion.
 *
 * @group EKF2
 * @min 0.5
 * @max 5.0
 * @unit m/s
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_EAS_NOISE, 1.4f);

/**
 * Noise for synthetic sideslip fusion.
 *
 * @group EKF2
 * @min 0.1
 * @max 1.0
 * @unit m/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_BETA_NOISE, 0.3f);

/**
 * Magnetic declination
 *
 * @group EKF2
 * @unit deg
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_DECL, 0);

/**
 * Gate size for magnetic heading fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_HDG_GATE, 2.6f);

/**
 * Gate size for magnetometer XYZ component fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_MAG_GATE, 3.0f);

/**
 * Integer bitmask controlling handling of magnetic declination.
 *
 * Set bits in the following positions to enable functions.
 * 0 : Set to true to use the declination from the geo_lookup library when the GPS position becomes available, set to false to always use the EKF2_MAG_DECL value.
 * 1 : Set to true to save the EKF2_MAG_DECL parameter to the value returned by the EKF when the vehicle disarms.
 * 2 : Set to true to always use the declination as an observation when 3-axis magnetometer fusion is being used.
 *
 * @group EKF2
 * @min 0
 * @max 7
 * @bit 0 use geo_lookup declination
 * @bit 1 save EKF2_MAG_DECL on disarm
 * @bit 2 use declination as an observation
 */
PARAM_DEFINE_INT32(EKF2_DECL_TYPE, 7);

/**
 * Type of magnetometer fusion
 *
 * Integer controlling the type of magnetometer fusion used - magnetic heading or 3-axis magnetometer.
 * If set to automatic: heading fusion on-ground and 3-axis fusion in-flight
 *
 * @group EKF2
 * @value 0 Automatic
 * @value 1 Magnetic heading
 * @value 2 3-axis fusion
 * @value 3 None
 */
PARAM_DEFINE_INT32(EKF2_MAG_TYPE, 0);

/**
 * Gate size for barometric height fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_BARO_GATE, 5.0f);

/**
 * Gate size for GPS horizontal position fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_P_GATE, 5.0f);

/**
 * Gate size for GPS velocity fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_V_GATE, 5.0f);

/**
 * Gate size for TAS fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_TAS_GATE, 3.0f);

/**
 * Replay mode
 *
 * A value of 1 indicates that the ekf2 module will publish
 * replay messages for logging.
 *
 * @group EKF2
 * @boolean
 */
PARAM_DEFINE_INT32(EKF2_REC_RPL, 0);

/**
 * Integer bitmask controlling data fusion and aiding methods.
 *
 * Set bits in the following positions to enable:
 * 0 : Set to true to use GPS data if available
 * 1 : Set to true to use optical flow data if available
 * 2 : Set to true to inhibit IMU bias estimation
 * 3 : Set to true to enable vision position fusion
 * 4 : Set to true to enable vision yaw fusion
 *
 * @group EKF2
 * @min 0
 * @max 28
 * @bit 0 use GPS
 * @bit 1 use optical flow
 * @bit 2 inhibit IMU bias estimation
 * @bit 3 vision position fusion
 * @bit 4 vision yaw fusion
 */
PARAM_DEFINE_INT32(EKF2_AID_MASK, 1);

/**
 * Determines the primary source of height data used by the EKF.
 *
 * The range sensor option should only be used when for operation over a flat surface as the local NED origin will move up and down with ground level.
 *
 * @group EKF2
 * @value 0 Barometric pressure
 * @value 1 GPS
 * @value 2 Range sensor
 * @value 3 Vision
 *
 */
PARAM_DEFINE_INT32(EKF2_HGT_MODE, 0);

/**
 * Measurement noise for range finder fusion
 *
 * @group EKF2
 * @min 0.01
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_NOISE, 0.1f);

/**
 * Gate size for range finder fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_GATE, 5.0f);

/**
 * Minimum valid range for the range finder
 *
 * @group EKF2
 * @min 0.01
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_MIN_RNG, 0.1f);


/**
 * Measurement noise for vision position observations used when the vision system does not supply error estimates
 *
 * @group EKF2
 * @min 0.01
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_EVP_NOISE, 0.05f);

/**
 * Measurement noise for vision angle observations used when the vision system does not supply error estimates
 *
 * @group EKF2
 * @min 0.01
 * @unit rad
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_EVA_NOISE, 0.05f);

/**
 * Gate size for vision estimate fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_EV_GATE, 5.0f);

/**
 * Minimum valid range for the vision estimate
 *
 * @group EKF2
 * @min 0.01
 * @unit m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_MIN_EV, 0.01f);
/**
 * Measurement noise for the optical flow sensor when it's reported quality metric is at the maximum
 *
 * @group EKF2
 * @min 0.05
 * @unit rad/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_OF_N_MIN, 0.15f);

/**
 * Measurement noise for the optical flow sensor.
 *
 * (when it's reported quality metric is at the minimum set by EKF2_OF_QMIN).
 * The following condition must be met: EKF2_OF_N_MAXN >= EKF2_OF_N_MIN
 *
 * @group EKF2
 * @min 0.05
 * @unit rad/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_OF_N_MAX, 0.5f);

/**
 * Optical Flow data will only be used if the sensor reports a quality metric >= EKF2_OF_QMIN.
 *
 * @group EKF2
 * @min 0
 * @max 255
 */
PARAM_DEFINE_INT32(EKF2_OF_QMIN, 1);

/**
 * Gate size for optical flow fusion
 *
 * @group EKF2
 * @min 1.0
 * @unit SD
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_OF_GATE, 3.0f);

/**
 * Optical Flow data will not fused if the magnitude of the flow rate > EKF2_OF_RMAX
 *
 * @group EKF2
 * @min 1.0
 * @unit rad/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_OF_RMAX, 2.5f);

/**
 * Terrain altitude process noise - accounts for instability in vehicle height estimate
 *
 * @group EKF2
 * @min 0.5
 * @unit m/s
 * @decimal 1
 */
PARAM_DEFINE_FLOAT(EKF2_TERR_NOISE, 5.0f);

/**
 * Magnitude of terrain gradient
 *
 * @group EKF2
 * @min 0.0
 * @unit m/m
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_TERR_GRAD, 0.5f);

/**
 * X position of IMU in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_IMU_POS_X, 0.0f);

/**
 * Y position of IMU in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_IMU_POS_Y, 0.0f);

/**
 * Z position of IMU in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_IMU_POS_Z, 0.0f);

/**
 * X position of GPS antenna in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_POS_X, 0.0f);

/**
 * Y position of GPS antenna in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_POS_Y, 0.0f);

/**
 * Z position of GPS antenna in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_GPS_POS_Z, 0.0f);

/**
 * X position of range finder origin in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_POS_X, 0.0f);

/**
 * Y position of range finder origin in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_POS_Y, 0.0f);

/**
 * Z position of range finder origin in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_RNG_POS_Z, 0.0f);

/**
 * X position of optical flow focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_OF_POS_X, 0.0f);

/**
 * Y position of optical flow focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_OF_POS_Y, 0.0f);

/**
 * Z position of optical flow focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_OF_POS_Z, 0.0f);

/**
* X position of VI sensor focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_EV_POS_X, 0.0f);

/**
 * Y position of VI sensor focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_EV_POS_Y, 0.0f);

/**
 * Z position of VI sensor focal point in body frame
 *
 * @group EKF2
 * @unit m
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_EV_POS_Z, 0.0f);

/**
* Airspeed fusion threshold. A value of zero will deactivate airspeed fusion. Any other positive
* value will determine the minimum airspeed which will still be fused.
*
* @group EKF2
* @min 0.0
* @unit m/s
* @decimal 1
*/
PARAM_DEFINE_FLOAT(EKF2_ARSP_THR, 0.0f);

/**
* Boolean determining if synthetic sideslip measurements should fused.
*
* A value of 1 indicates that fusion is active
*
* @group EKF2
* @boolean
*/
PARAM_DEFINE_INT32(EKF2_FUSE_BETA, 0);

/**

 * Time constant of the velocity output prediction and smoothing filter
 *
 * @group EKF2
 * @max 1.0
 * @unit s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_TAU_VEL, 0.25f);

/**
 * Time constant of the position output prediction and smoothing filter. Controls how tightly the output track the EKF states.
 *
 * @group EKF2
 * @min 0.1
 * @max 1.0
 * @unit s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_TAU_POS, 0.25f);

/**
 * 1-sigma IMU gyro switch-on bias
 *
 * @group EKF2
 * @min 0.0
 * @max 0.2
 * @unit rad/sec
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_GBIAS_INIT, 0.1f);

/**
 * 1-sigma IMU accelerometer switch-on bias
 *
 * @group EKF2
 * @min 0.0
 * @max 0.5
 * @unit m/s/s
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(EKF2_ABIAS_INIT, 0.2f);

/**
 * 1-sigma tilt angle uncertainty after gravity vector alignment
 *
 * @group EKF2
 * @min 0.0
 * @max 0.5
 * @unit rad
 * @decimal 3
 */
PARAM_DEFINE_FLOAT(EKF2_ANGERR_INIT, 0.1f);
