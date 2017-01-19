/****************************************************************************
 *
 *   Copyright (c) 2016 PX4 Development Team. All rights reserved.
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
 * @file temperature_compensation.cpp
 *
 * Sensors temperature compensation methods
 *
 * @author Paul Riseborough <gncsolns@gmail.com>
 */

#include "temperature_compensation.h"
#include <systemlib/param/param.h>
#include <stdio.h>
#include <px4_defines.h>

namespace sensors_temp_comp
{

int initialize_parameter_handles(ParameterHandles &parameter_handles)
{
	char nbuf[16];

	/* rate gyro calibration parameters */
	parameter_handles.gyro_tc_enable = param_find("TC_G_ENABLE");

	for (unsigned j = 0; j < 3; j++) {
		sprintf(nbuf, "TC_G%d_ID", j);
		parameter_handles.gyro_cal_handles[j].ID = param_find(nbuf);

		for (unsigned i = 0; i < 3; i++) {
			sprintf(nbuf, "TC_G%d_X3_%d", j, i);
			parameter_handles.gyro_cal_handles[j].x3[i] = param_find(nbuf);
			sprintf(nbuf, "TC_G%d_X2_%d", j, i);
			parameter_handles.gyro_cal_handles[j].x2[i] = param_find(nbuf);
			sprintf(nbuf, "TC_G%d_X1_%d", j, i);
			parameter_handles.gyro_cal_handles[j].x1[i] = param_find(nbuf);
			sprintf(nbuf, "TC_G%d_X0_%d", j, i);
			parameter_handles.gyro_cal_handles[j].x0[i] = param_find(nbuf);
			sprintf(nbuf, "TC_G%d_SCL_%d", j, i);
			parameter_handles.gyro_cal_handles[j].scale[i] = param_find(nbuf);
		}

		sprintf(nbuf, "TC_G%d_TREF", j);
		parameter_handles.gyro_cal_handles[j].ref_temp = param_find(nbuf);
		sprintf(nbuf, "TC_G%d_TMIN", j);
		parameter_handles.gyro_cal_handles[j].min_temp = param_find(nbuf);
		sprintf(nbuf, "TC_G%d_TMAX", j);
		parameter_handles.gyro_cal_handles[j].max_temp = param_find(nbuf);
	}

	/* accelerometer calibration parameters */
	parameter_handles.accel_tc_enable = param_find("TC_A_ENABLE");

	for (unsigned j = 0; j < 3; j++) {
		sprintf(nbuf, "TC_A%d_ID", j);
		parameter_handles.accel_cal_handles[j].ID = param_find(nbuf);

		for (unsigned i = 0; i < 3; i++) {
			sprintf(nbuf, "TC_A%d_X3_%d", j, i);
			parameter_handles.accel_cal_handles[j].x3[i] = param_find(nbuf);
			sprintf(nbuf, "TC_A%d_X2_%d", j, i);
			parameter_handles.accel_cal_handles[j].x2[i] = param_find(nbuf);
			sprintf(nbuf, "TC_A%d_X1_%d", j, i);
			parameter_handles.accel_cal_handles[j].x1[i] = param_find(nbuf);
			sprintf(nbuf, "TC_A%d_X0_%d", j, i);
			parameter_handles.accel_cal_handles[j].x0[i] = param_find(nbuf);
			sprintf(nbuf, "TC_A%d_SCL_%d", j, i);
			parameter_handles.accel_cal_handles[j].scale[i] = param_find(nbuf);
		}

		sprintf(nbuf, "TC_A%d_TREF", j);
		parameter_handles.accel_cal_handles[j].ref_temp = param_find(nbuf);
		sprintf(nbuf, "TC_A%d_TMIN", j);
		parameter_handles.accel_cal_handles[j].min_temp = param_find(nbuf);
		sprintf(nbuf, "TC_A%d_TMAX", j);
		parameter_handles.accel_cal_handles[j].max_temp = param_find(nbuf);
	}

	/* barometer calibration parameters */
	parameter_handles.baro_tc_enable = param_find("TC_B_ENABLE");

	for (unsigned j = 0; j < 3; j++) {
		sprintf(nbuf, "TC_B%d_ID", j);
		parameter_handles.baro_cal_handles[j].ID = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X5", j);
		parameter_handles.baro_cal_handles[j].x5 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X4", j);
		parameter_handles.baro_cal_handles[j].x4 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X3", j);
		parameter_handles.baro_cal_handles[j].x3 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X2", j);
		parameter_handles.baro_cal_handles[j].x2 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X1", j);
		parameter_handles.baro_cal_handles[j].x1 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_X0", j);
		parameter_handles.baro_cal_handles[j].x0 = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_SCL", j);
		parameter_handles.baro_cal_handles[j].scale = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_TREF", j);
		parameter_handles.baro_cal_handles[j].ref_temp = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_TMIN", j);
		parameter_handles.baro_cal_handles[j].min_temp = param_find(nbuf);
		sprintf(nbuf, "TC_B%d_TMAX", j);
		parameter_handles.baro_cal_handles[j].max_temp = param_find(nbuf);
	}

	return PX4_OK;
}

int update_parameters(const ParameterHandles &parameter_handles, Parameters &parameters)
{
	int ret = PX4_OK;

	/* rate gyro calibration parameters */
	param_get(parameter_handles.gyro_tc_enable, &(parameters.gyro_tc_enable));

	for (unsigned j = 0; j < 3; j++) {
		if (param_get(parameter_handles.gyro_cal_handles[j].ID, &(parameters.gyro_cal_data[j].ID)) == PX4_OK) {
			param_get(parameter_handles.gyro_cal_handles[j].ref_temp, &(parameters.gyro_cal_data[j].ref_temp));
			param_get(parameter_handles.gyro_cal_handles[j].min_temp, &(parameters.gyro_cal_data[j].min_temp));
			param_get(parameter_handles.gyro_cal_handles[j].min_temp, &(parameters.gyro_cal_data[j].min_temp));

			for (unsigned int i = 0; i < 3; i++) {
				param_get(parameter_handles.gyro_cal_handles[j].x3[i], &(parameters.gyro_cal_data[j].x3[i]));
				param_get(parameter_handles.gyro_cal_handles[j].x2[i], &(parameters.gyro_cal_data[j].x2[i]));
				param_get(parameter_handles.gyro_cal_handles[j].x1[i], &(parameters.gyro_cal_data[j].x1[i]));
				param_get(parameter_handles.gyro_cal_handles[j].x0[i], &(parameters.gyro_cal_data[j].x0[i]));
				param_get(parameter_handles.gyro_cal_handles[j].scale[i], &(parameters.gyro_cal_data[j].scale[i]));
			}

		} else {
			// Set all cal values to zero and scale factor to unity
			memset(&parameters.gyro_cal_data[j], 0, sizeof(parameters.gyro_cal_data[j]));

			// Set the scale factor to unity
			for (unsigned int i = 0; i < 3; i++) {
				parameters.gyro_cal_data[j].scale[i] = 1.0f;
			}

			PX4_WARN("FAIL GYRO %d CAL PARAM LOAD - USING DEFAULTS", j);
			ret = PX4_ERROR;
		}
	}

	/* accelerometer calibration parameters */
	param_get(parameter_handles.accel_tc_enable, &(parameters.accel_tc_enable));

	for (unsigned j = 0; j < 3; j++) {
		if (param_get(parameter_handles.accel_cal_handles[j].ID, &(parameters.accel_cal_data[j].ID)) == PX4_OK) {
			param_get(parameter_handles.accel_cal_handles[j].ref_temp, &(parameters.accel_cal_data[j].ref_temp));
			param_get(parameter_handles.accel_cal_handles[j].min_temp, &(parameters.accel_cal_data[j].min_temp));
			param_get(parameter_handles.accel_cal_handles[j].min_temp, &(parameters.accel_cal_data[j].min_temp));

			for (unsigned int i = 0; i < 3; i++) {
				param_get(parameter_handles.accel_cal_handles[j].x3[i], &(parameters.accel_cal_data[j].x3[i]));
				param_get(parameter_handles.accel_cal_handles[j].x2[i], &(parameters.accel_cal_data[j].x2[i]));
				param_get(parameter_handles.accel_cal_handles[j].x1[i], &(parameters.accel_cal_data[j].x1[i]));
				param_get(parameter_handles.accel_cal_handles[j].x0[i], &(parameters.accel_cal_data[j].x0[i]));
				param_get(parameter_handles.accel_cal_handles[j].scale[i], &(parameters.accel_cal_data[j].scale[i]));
			}

		} else {
			// Set all cal values to zero and scale factor to unity
			memset(&parameters.accel_cal_data[j], 0, sizeof(parameters.accel_cal_data[j]));

			// Set the scale factor to unity
			for (unsigned int i = 0; i < 3; i++) {
				parameters.accel_cal_data[j].scale[i] = 1.0f;
			}

			PX4_WARN("FAIL ACCEL %d CAL PARAM LOAD - USING DEFAULTS", j);
			ret = PX4_ERROR;
		}
	}

	/* barometer calibration parameters */
	param_get(parameter_handles.baro_tc_enable, &(parameters.baro_tc_enable));

	for (unsigned j = 0; j < 3; j++) {
		if (param_get(parameter_handles.baro_cal_handles[j].ID, &(parameters.baro_cal_data[j].ID)) == PX4_OK) {
			param_get(parameter_handles.baro_cal_handles[j].ref_temp, &(parameters.baro_cal_data[j].ref_temp));
			param_get(parameter_handles.baro_cal_handles[j].min_temp, &(parameters.baro_cal_data[j].min_temp));
			param_get(parameter_handles.baro_cal_handles[j].min_temp, &(parameters.baro_cal_data[j].min_temp));
			param_get(parameter_handles.baro_cal_handles[j].x5, &(parameters.baro_cal_data[j].x5));
			param_get(parameter_handles.baro_cal_handles[j].x4, &(parameters.baro_cal_data[j].x4));
			param_get(parameter_handles.baro_cal_handles[j].x3, &(parameters.baro_cal_data[j].x3));
			param_get(parameter_handles.baro_cal_handles[j].x2, &(parameters.baro_cal_data[j].x2));
			param_get(parameter_handles.baro_cal_handles[j].x1, &(parameters.baro_cal_data[j].x1));
			param_get(parameter_handles.baro_cal_handles[j].x0, &(parameters.baro_cal_data[j].x0));
			param_get(parameter_handles.baro_cal_handles[j].scale, &(parameters.baro_cal_data[j].scale));

		} else {
			// Set all cal values to zero and scale factor to unity
			memset(&parameters.baro_cal_data[j], 0, sizeof(parameters.baro_cal_data[j]));

			// Set the scale factor to unity
			parameters.baro_cal_data[j].scale = 1.0f;

			PX4_WARN("FAIL BARO %d CAL PARAM LOAD - USING DEFAULTS", j);
			ret = PX4_ERROR;
		}
	}	return ret;
}

bool calc_thermal_offsets_1D(struct SENSOR_CAL_DATA_1D &coef, const float &measured_temp, float &offset)
{
	bool ret = true;

	// clip the measured temperature to remain within the calibration range
	float delta_temp;

	if (measured_temp > coef.max_temp) {
		delta_temp = coef.max_temp - coef.ref_temp;
		ret = false;

	} else if (measured_temp < coef.min_temp) {
		delta_temp = coef.min_temp - coef.ref_temp;
		ret = false;

	} else {
		delta_temp = measured_temp - coef.ref_temp;

	}

	delta_temp = measured_temp - coef.ref_temp;

	// calulate the offset
	offset = coef.x0 + coef.x1 * delta_temp;
	delta_temp *= delta_temp;
	offset += coef.x2 * delta_temp;
	delta_temp *= delta_temp;
	offset += coef.x3 * delta_temp;
	delta_temp *= delta_temp;
	offset += coef.x4 * delta_temp;
	delta_temp *= delta_temp;
	offset += coef.x5 * delta_temp;

	return ret;

}

bool calc_thermal_offsets_3D(struct SENSOR_CAL_DATA_3D &coef, const float &measured_temp, float offset[])
{
	bool ret = true;

	// clip the measured temperature to remain within the calibration range
	float delta_temp;

	if (measured_temp > coef.max_temp) {
		delta_temp = coef.max_temp;
		ret = false;

	} else if (measured_temp < coef.min_temp) {
		delta_temp = coef.min_temp;
		ret = false;

	} else {
		delta_temp = measured_temp;

	}

	delta_temp -= coef.ref_temp;

	// calulate the offsets
	float delta_temp_2 = delta_temp * delta_temp;
	float delta_temp_3 = delta_temp_2 * delta_temp;

	for (uint8_t i = 0; i < 3; i++) {
		offset[i] = coef.x0[i] + coef.x1[i] * delta_temp + coef.x2[i] * delta_temp_2 + coef.x3[i] * delta_temp_3;

	}

	return ret;

}

}
