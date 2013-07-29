#include "BlockSegwayController.hpp"

void BlockSegwayController::update() {
	// wait for a sensor update, check for exit condition every 100 ms
	if (poll(&_attPoll, 1, 100) < 0) return; // poll error

	uint64_t newTimeStamp = hrt_absolute_time();
	float dt = (newTimeStamp - _timeStamp) / 1.0e6f;
	_timeStamp = newTimeStamp;

	// check for sane values of dt
	// to prevent large control responses
	if (dt > 1.0f || dt < 0) return;

	// set dt for all child blocks
	setDt(dt);

	// check for new updates
	if (_param_update.updated()) updateParams();

	// get new information from subscriptions
	updateSubscriptions();

	// default all output to zero unless handled by mode
	for (unsigned i = 2; i < NUM_ACTUATOR_CONTROLS; i++)
		_actuators.control[i] = 0.0f;

	// only update guidance in auto mode
	if (_status.state_machine == SYSTEM_STATE_AUTO) {
		// update guidance
	}

	// compute speed command
	float spdCmd = -theta2spd.update(_att.pitch) - q2spd.update(_att.pitchspeed);

	// handle autopilot modes
	if (_status.state_machine == SYSTEM_STATE_AUTO ||
	    _status.state_machine == SYSTEM_STATE_STABILIZED) {
		_actuators.control[0] = spdCmd;
		_actuators.control[1] = spdCmd;

	} else if (_status.state_machine == SYSTEM_STATE_MANUAL) {
		if (_status.manual_control_mode == VEHICLE_MANUAL_CONTROL_MODE_DIRECT) {
			_actuators.control[CH_LEFT] = _manual.throttle;
			_actuators.control[CH_RIGHT] = _manual.pitch;

		} else if (_status.manual_control_mode == VEHICLE_MANUAL_CONTROL_MODE_SAS) {
			_actuators.control[0] = spdCmd;
			_actuators.control[1] = spdCmd;
		}
	}

	// update all publications
	updatePublications();

}

