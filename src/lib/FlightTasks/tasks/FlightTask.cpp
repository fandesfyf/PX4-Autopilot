#include "FlightTask.hpp"
#include <mathlib/mathlib.h>

constexpr uint64_t FlightTask::_timeout;
// First index of empty_setpoint corresponds to time-stamp and requires a finite number.
const vehicle_local_position_setpoint_s FlightTask::empty_setpoint = {0, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, {NAN, NAN, NAN}};
const vehicle_constraints_s FlightTask::empty_constraints = {0, NAN, NAN, NAN};
bool FlightTask::initializeSubscriptions(SubscriptionArray &subscription_array)
{
	if (!subscription_array.get(ORB_ID(vehicle_local_position), _sub_vehicle_local_position)) {
		return false;
	}

	return true;
}

bool FlightTask::activate()
{
	_resetSetpoints();
	_setDefaultConstraints();
	_time_stamp_activate = hrt_absolute_time();
	return true;
}

bool FlightTask::updateInitialize()
{
	_time_stamp_current = hrt_absolute_time();
	_time = (_time_stamp_current - _time_stamp_activate) / 1e6f;
	_deltatime  = math::min((_time_stamp_current - _time_stamp_last), _timeout) / 1e6f;
	_time_stamp_last = _time_stamp_current;
	return _evaluateVehicleLocalPosition();
}

const vehicle_local_position_setpoint_s FlightTask::getPositionSetpoint()
{
	/* fill position setpoint message */
	vehicle_local_position_setpoint_s vehicle_local_position_setpoint;
	vehicle_local_position_setpoint.timestamp = hrt_absolute_time();
	_position_setpoint.copyToRaw(&vehicle_local_position_setpoint.x);
	_velocity_setpoint.copyToRaw(&vehicle_local_position_setpoint.vx);
	_acceleration_setpoint.copyToRaw(&vehicle_local_position_setpoint.acc_x);
	_thrust_setpoint.copyTo(vehicle_local_position_setpoint.thrust);
	vehicle_local_position_setpoint.yaw = _yaw_setpoint;
	vehicle_local_position_setpoint.yawspeed = _yawspeed_setpoint;
	return vehicle_local_position_setpoint;
}

void FlightTask::_resetSetpoints()
{
	_position_setpoint *= NAN;
	_velocity_setpoint *= NAN;
	_acceleration_setpoint *= NAN;
	_thrust_setpoint *= NAN;
	_yaw_setpoint = _yawspeed_setpoint = NAN;
}

bool FlightTask::_evaluateVehicleLocalPosition()
{
	if ((_time_stamp_current - _sub_vehicle_local_position->get().timestamp) < _timeout) {
		_position = matrix::Vector3f(&_sub_vehicle_local_position->get().x);
		_velocity = matrix::Vector3f(&_sub_vehicle_local_position->get().vx);
		_yaw = _sub_vehicle_local_position->get().yaw;
		_dist_to_bottom = NAN;

		if (_sub_vehicle_local_position->get().dist_bottom_valid) {
			_dist_to_bottom =  _sub_vehicle_local_position->get().dist_bottom;
		}

		return true;

	} else {
		_resetSetpoints();
		_velocity.zero(); /* default velocity is all zero */
		return false;
	}
}

void FlightTask::_setDefaultConstraints()
{
	_constraints.speed_xy = MPC_XY_VEL_MAX.get();
	_constraints.speed_up = MPC_Z_VEL_MAX_UP.get();
	_constraints.speed_down = MPC_Z_VEL_MAX_DN.get();
	_constraints.tilt = math::radians(MPC_TILTMAX_AIR.get());
	_constraints.landing_gear_up = false;
}
