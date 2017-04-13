#include "camera_interface.h"

/**
 * @file camera_interface.cpp
 *
 */

CameraInterface::CameraInterface():
	_pins{}
{
}

CameraInterface::~CameraInterface()
{
}

void CameraInterface::get_pins()
{

	// Get parameter handle
	_p_pin = param_find("TRIG_PINS");

	int pin_list;
	param_get(_p_pin, &pin_list);

	// Set all pins as invalid
	for (unsigned i = 0; i < arraySize(_pins); i++) {
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

}