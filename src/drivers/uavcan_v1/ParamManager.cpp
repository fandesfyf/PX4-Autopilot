/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
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
 * @file ParamManager.cpp
 *
 * Implements basic functionality of UAVCAN parameter management class
 *
 * @author Jacob Crabill <jacob@flyvoly.com>
 */

#include "ParamManager.hpp"

bool UavcanParamManager::GetParamByName(const char *param_name, uavcan_register_Value_1_0 &value)
{
	for (auto &param : _uavcan_params) {
		if (strcmp(param_name, param.uavcan_name) == 0) {
			param_t param_handle = param_find(param.px4_name);

			if (param_handle == PARAM_INVALID) {
				return false;
			}

			/// TODO: What will be our approach for handling other UAVCAN data-value types?
			switch (param_type(param_handle)) {
			case PARAM_TYPE_INT32: {
					int32_t out_val {};
					param_get(param_handle, &out_val);
					value.integer32.value.elements[0] = out_val;
					uavcan_register_Value_1_0_select_integer32_(&value);
					break;
				}

			case PARAM_TYPE_FLOAT: {
					float out_val {};
					param_get(param_handle, &out_val);
					value.natural32.value.elements[0] = out_val;
					uavcan_register_Value_1_0_select_natural32_(&value);
					break;
				}
			}

			return true;
		}
	}

	return false;
}

bool UavcanParamManager::GetParamByName(const uavcan_register_Name_1_0 &name, uavcan_register_Value_1_0 &value)
{
	for (auto &param : _uavcan_params) {
		if (strncmp((char *)name.name.elements, param.uavcan_name, name.name.count) == 0) {
			param_t param_handle = param_find(param.px4_name);

			if (param_handle == PARAM_INVALID) {
				return false;
			}

			/// TODO: What will be our approach for handling other UAVCAN data-value types?
			switch (param_type(param_handle)) {
			case PARAM_TYPE_INT32: {
					int32_t out_val {};
					param_set(param_handle, &out_val);
					value.integer32.value.elements[0] = out_val;
					uavcan_register_Value_1_0_select_integer32_(&value);
					break;
				}

			case PARAM_TYPE_FLOAT: {
					float out_val {};
					param_set(param_handle, &out_val);
					value.natural32.value.elements[0] = out_val;
					uavcan_register_Value_1_0_select_natural32_(&value);
					break;
				}
			}

			return true;
		}
	}

	return false;
}

bool UavcanParamManager::SetParamByName(const uavcan_register_Name_1_0 &name, const uavcan_register_Value_1_0 &value)
{
	for (auto &param : _uavcan_params) {
		if (strncmp((char *)name.name.elements, param.uavcan_name, name.name.count) == 0) {
			param_t param_handle = param_find(param.px4_name);

			if (param_handle == PARAM_INVALID) {
				return false;
			}

			switch (param_type(param_handle)) {
			case PARAM_TYPE_INT32: {
					int32_t in_val = value.integer32.value.elements[0];
					param_set(param_handle, &in_val);
					break;
				}

			case PARAM_TYPE_FLOAT: {
					float in_val = value.natural32.value.elements[0];
					param_set(param_handle, &in_val);
					break;
				}
			}

			return true;
		}
	}

	return false;
}
