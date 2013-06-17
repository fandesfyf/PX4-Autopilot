/****************************************************************************
 *
 *   Copyright (c) 2013 PX4 Development Team. All rights reserved.
 *   Author: Anton Babushkin <rk3dov@gmail.com>
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
 * @file sdlog2_messages.h
 *
 * Log messages and structures definition.
 *
 * @author Anton Babushkin <rk3dov@gmail.com>
 */

#ifndef SDLOG2_MESSAGES_H_
#define SDLOG2_MESSAGES_H_

#include "sdlog2_format.h"

/* define message formats */

#pragma pack(push, 1)
/* --- TIME - TIME STAMP --- */
#define LOG_TIME_MSG 1
struct log_TIME_s {
	uint64_t t;
};

/* --- ATT - ATTITUDE --- */
#define LOG_ATT_MSG 2
struct log_ATT_s {
	float roll;
	float pitch;
	float yaw;
	float roll_rate;
	float pitch_rate;
	float yaw_rate;
};

/* --- ATSP - ATTITUDE SET POINT --- */
#define LOG_ATSP_MSG 3
struct log_ATSP_s {
	float roll_sp;
	float pitch_sp;
	float yaw_sp;
	float thrust_sp;
};

/* --- IMU - IMU SENSORS --- */
#define LOG_IMU_MSG 4
struct log_IMU_s {
	float acc_x;
	float acc_y;
	float acc_z;
	float gyro_x;
	float gyro_y;
	float gyro_z;
	float mag_x;
	float mag_y;
	float mag_z;
};

/* --- SENS - OTHER SENSORS --- */
#define LOG_SENS_MSG 5
struct log_SENS_s {
	float baro_pres;
	float baro_alt;
	float baro_temp;
	float diff_pres;
};

/* --- LPOS - LOCAL POSITION --- */
#define LOG_LPOS_MSG 6
struct log_LPOS_s {
	float x;
	float y;
	float z;
	float vx;
	float vy;
	float vz;
	float hdg;
	int32_t home_lat;
	int32_t home_lon;
	float home_alt;
};

/* --- LPSP - LOCAL POSITION SETPOINT --- */
#define LOG_LPSP_MSG 7
struct log_LPSP_s {
	float x;
	float y;
	float z;
	float yaw;
};

/* --- GPS - GPS POSITION --- */
#define LOG_GPS_MSG 8
struct log_GPS_s {
	uint64_t gps_time;
	uint8_t fix_type;
	float eph;
	float epv;
	int32_t lat;
	int32_t lon;
	float alt;
	float vel_n;
	float vel_e;
	float vel_d;
	float cog;
};

/* --- ATTC - ATTITUDE CONTROLS (ACTUATOR_0 CONTROLS)--- */
#define LOG_ATTC_MSG 9
struct log_ATTC_s {
	float roll;
	float pitch;
	float yaw;
	float thrust;
};

/* --- STAT - VEHICLE STATE --- */
#define LOG_STAT_MSG 10
struct log_STAT_s {
	unsigned char state;
	unsigned char flight_mode;
	unsigned char manual_control_mode;
	unsigned char manual_sas_mode;
	unsigned char armed;
	float battery_voltage;
	float battery_current;
	float battery_remaining;
	unsigned char battery_warning;
};

/* --- RC - RC INPUT CHANNELS --- */
#define LOG_RC_MSG 11
struct log_RC_s {
	float channel[8];
};

/* --- OUT0 - ACTUATOR_0 OUTPUT --- */
#define LOG_OUT0_MSG 12
struct log_OUT0_s {
	float output[8];
};

/* --- AIRS - AIRSPEED --- */
#define LOG_AIRS_MSG 13
struct log_AIRS_s {
	float indicated_airspeed;
	float true_airspeed;
};

/* --- ARSP - ATTITUDE RATE SET POINT --- */
#define LOG_ARSP_MSG 14
struct log_ARSP_s {
	float roll_rate_sp;
	float pitch_rate_sp;
	float yaw_rate_sp;
};

/* --- GPOS - GLOBAL POSITION --- */
#define LOG_GPOS_MSG 15
struct log_GPOS_s {
	int32_t lat;
	int32_t lon;
	float alt;
	float vel_n;
	float vel_e;
	float vel_d;
	float hdg;
};

#pragma pack(pop)

/* construct list of all message formats */

static const struct log_format_s log_formats[] = {
	LOG_FORMAT(TIME, "Q", "StartTime"),
	LOG_FORMAT(ATT, "ffffff", "Roll,Pitch,Yaw,RollRate,PitchRate,YawRate"),
	LOG_FORMAT(ATSP, "ffff", "RollSP,PitchSP,YawSP,ThrustSP"),
	LOG_FORMAT(IMU, "fffffffff", "AccX,AccY,AccZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ"),
	LOG_FORMAT(SENS, "ffff", "BaroPres,BaroAlt,BaroTemp,DiffPres"),
	LOG_FORMAT(LPOS, "fffffffLLf", "X,Y,Z,VX,VY,VZ,Heading,HomeLat,HomeLon,HomeAlt"),
	LOG_FORMAT(LPSP, "ffff", "X,Y,Z,Yaw"),
	LOG_FORMAT(GPS, "QBffLLfffff", "GPSTime,FixType,EPH,EPV,Lat,Lon,Alt,VelN,VelE,VelD,Cog"),
	LOG_FORMAT(ATTC, "ffff", "Roll,Pitch,Yaw,Thrust"),
	LOG_FORMAT(STAT, "BBBBBfffB", "State,FlightMode,CtlMode,SASMode,Armed,BatV,BatC,BatRem,BatWarn"),
	LOG_FORMAT(RC, "ffffffff", "Ch0,Ch1,Ch2,Ch3,Ch4,Ch5,Ch6,Ch7"),
	LOG_FORMAT(OUT0, "ffffffff", "Out0,Out1,Out2,Out3,Out4,Out5,Out6,Out7"),
	LOG_FORMAT(AIRS, "ff", "IndSpeed,TrueSpeed"),
	LOG_FORMAT(ARSP, "fff", "RollRateSP,PitchRateSP,YawRateSP"),
	LOG_FORMAT(GPOS, "LLfffff", "Lat,Lon,Alt,VelN,VelE,VelD,Heading"),
};

static const int log_formats_num = sizeof(log_formats) / sizeof(struct log_format_s);

#endif /* SDLOG2_MESSAGES_H_ */
