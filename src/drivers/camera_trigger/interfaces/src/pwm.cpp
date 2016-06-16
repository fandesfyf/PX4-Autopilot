#include <px4.h>
#include <sys/ioctl.h>
#include <lib/mathlib/mathlib.h>

#include "drivers/drv_pwm_output.h"
#include "pwm.h"

// PWM levels of the interface to seagull MAP converter to
// Multiport (http://www.seagulluav.com/manuals/Seagull_MAP2-Manual.pdf)
#define PWM_CAMERA_DISARMED			90 // TODO(birchera): check here value
#define PWM_CAMERA_ON				1100
#define PWM_CAMERA_AUTOFOCUS_SHOOT	1300
#define PWM_CAMERA_NEUTRAL			1500
#define PWM_CAMERA_INSTANT_SHOOT	1700
#define PWM_CAMERA_OFF				1900

CameraInterfacePWM::CameraInterfacePWM():
	CameraInterface(),
	_camera_is_on(false)
{
	_p_pin = param_find("TRIG_PINS");
	int pin_list;
	param_get(_p_pin, &pin_list);

	// Set all pins as invalid
	for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
		_pins[i] = -1;
	}

	// Convert number to individual channels
	unsigned i = 0;
	int single_pin;

	while ((single_pin = pin_list % 10)) {

		_pins[i] = single_pin - 1;

		if (_pins[i] < 0) {
			_pins[i] = -1;
		}

		pin_list /= 10;
		i++;
	}

	setup();
}

CameraInterfacePWM::~CameraInterfacePWM()
{
}

void CameraInterfacePWM::setup()
{
	for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
		if (_pins[i] >= 0) {
			// TODO(birchera): use return value to make sure everything is fine
			up_pwm_servo_set(_pins[i], math::constrain(PWM_CAMERA_DISARMED, PWM_CAMERA_DISARMED, 2000));
		}
	}
}

void CameraInterfacePWM::trigger(bool enable)
{
	// This only starts working upon prearming

	if (!_camera_is_on) {
		// Turn camera on and give time to start up
		powerOn();
		return;
	}

	if (enable) {
		// Set all valid pins to shoot level
		for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
			if (_pins[i] >= 0) {
				up_pwm_servo_set(_pins[i], math::constrain(PWM_CAMERA_INSTANT_SHOOT, 1000, 2000));
			}
		}

	} else {
		// Set all valid pins back to neutral level
		for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
			if (_pins[i] >= 0) {
				up_pwm_servo_set(_pins[i], math::constrain(PWM_CAMERA_NEUTRAL, 1000, 2000));
			}
		}
	}
}

int CameraInterfacePWM::powerOn()
{
	// This only starts working upon prearming

	// Set all valid pins to turn on level
	for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
		if (_pins[i] >= 0) {
			up_pwm_servo_set(_pins[i], math::constrain(PWM_CAMERA_ON, 1000, 2000));
		}
	}

	_camera_is_on = true;

	return 0;
}

int CameraInterfacePWM::powerOff()
{
	// This only starts working upon prearming

	// Set all valid pins to turn off level
	for (unsigned i = 0; i < sizeof(_pins) / sizeof(_pins[0]); i++) {
		if (_pins[i] >= 0) {
			up_pwm_servo_set(_pins[i], math::constrain(PWM_CAMERA_OFF, 1000, 2000));
		}
	}

	_camera_is_on = false;

	return 0;
}

void CameraInterfacePWM::info()
{
	warnx("PWM - camera triggering, pins 1-3 : %d,%d,%d", _pins[0], _pins[1], _pins[2]);
}
