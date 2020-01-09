/****************************************************************************
 *
 *   Copyright (c) 2019 ECL Development Team. All rights reserved.
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
 * Test the fusion of airspeed data
 * @author Kamil Ritz <ka.ritz@hotmail.com>
 */

#include <gtest/gtest.h>
#include "EKF/ekf.h"
#include "sensor_simulator/sensor_simulator.h"
#include "sensor_simulator/ekf_wrapper.h"


class EkfAirspeedTest : public ::testing::Test {
 public:

	EkfAirspeedTest(): ::testing::Test(),
	_ekf{std::make_shared<Ekf>()},
	_sensor_simulator(_ekf),
	_ekf_wrapper(_ekf) {};

	std::shared_ptr<Ekf> _ekf;
	SensorSimulator _sensor_simulator;
	EkfWrapper _ekf_wrapper;

	// Setup the Ekf with synthetic measurements
	void SetUp() override
	{
		_ekf->init(0);
		_sensor_simulator.runSeconds(2);
	}

	// Use this method to clean up any memory, network etc. after each test
	void TearDown() override
	{
	}
};

TEST_F(EkfAirspeedTest, temp)
{
	const Vector3f simulated_velocity(1.5f,0.0f,0.0f);
	_ekf_wrapper.enableExternalVisionVelocityFusion();
	_sensor_simulator._vio.setVelocity(simulated_velocity);
	_sensor_simulator.startExternalVision();

	_ekf->set_in_air_status(true);
	_sensor_simulator.startAirspeedSensor();
	_sensor_simulator._airspeed.setData(0.1f,0.1f);

	_sensor_simulator.runSeconds(40);


	filter_control_status_u control_status;
	_ekf->get_control_mode(&control_status.value);
	EXPECT_TRUE(control_status.flags.wind);

	EXPECT_FALSE(_ekf_wrapper.isIntendingExternalVisionPositionFusion());
	EXPECT_TRUE(_ekf_wrapper.isIntendingExternalVisionVelocityFusion());
	EXPECT_FALSE(_ekf_wrapper.isIntendingExternalVisionHeadingFusion());

	EXPECT_TRUE(_ekf->local_position_is_valid());
	EXPECT_FALSE(_ekf->global_position_is_valid());

	const Vector3f vel = _ekf_wrapper.getVelocity();
	const Vector2f vel_wind = _ekf_wrapper.getWindVelocity();


	EXPECT_TRUE(matrix::isEqual(vel, simulated_velocity));
	EXPECT_TRUE(matrix::isEqual(vel_wind, Vector2f{1.4f, 0.0f}));

}
