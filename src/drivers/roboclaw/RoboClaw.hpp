/****************************************************************************
 *
 *   Copyright (C) 2013 PX4 Development Team. All rights reserved.
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
 * @file RoboClas.hpp
 *
 * RoboClaw Motor Driver
 *
 * references:
 * http://downloads.orionrobotics.com/downloads/datasheets/motor_controller_robo_claw_R0401.pdf
 *
 */

#pragma once

#include <poll.h>
#include <stdio.h>
#include <uORB/SubscriptionPollable.hpp>
#include <uORB/topics/actuator_controls.h>
#include <drivers/device/i2c.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
//#include <px4.h>
#include <px4_work_queue/ScheduledWorkItem.hpp>

/**
 * This is a driver for the RoboClaw motor controller
 */
class RoboClaw : public px4::ScheduledWorkItem
{
public:

	static int roboclawTest(int argc, char *argv[]);

	/** control channels */
	enum e_channel {
		CH_VOLTAGE_LEFT = 0,
		CH_VOLTAGE_RIGHT
	};

	/**  motors */
	enum e_motor {
		MOTOR_1 = 0,
		MOTOR_2
	};

	/**
	 * constructor
	 * @param deviceName the name of the
	 * 	serial port e.g. "/dev/ttyS2"
	 * @param address the adddress  of the motor
	 * 	(selectable on roboclaw)
	 * @param pulsesPerRev # of encoder
	 *  pulses per revolution of wheel
	 */
	RoboClaw(const char *deviceName, uint16_t address,
		 uint16_t pulsesPerRev);

	/**
	 * deconstructor
	 */
	virtual ~RoboClaw();

	/**
	 * @return position of a motor, rev
	 */
	float getMotorPosition(e_motor motor);

	/**
	 * @return speed of a motor, rev/sec
	 */
	float getMotorSpeed(e_motor motor);

	/**
	 * set the speed of a motor, rev/sec
	 */
	int setMotorSpeed(e_motor motor, float value);

	/**
	 * set the duty cycle of a motor
	 */
	int setMotorDutyCycle(e_motor motor, float value);

	/**
	 * Drive both motors. +1 = full forward, -1 = full backward
	 */
	int drive(float value);

	/**
	 * Turn. +1 = full right, -1 = full left
	 */
	int turn(float value);

	/**
	 * reset the encoders
	 * @return status
	 */
	int resetEncoders();

	/**
	 * main update loop that updates RoboClaw motor
	 * dutycycle based on actuator publication
	 */
	int update();

	/**
	 * read data from serial
	 */
	int readEncoder(e_motor motor);

	/**
	 * print status
	 */
	void printStatus(char *string, size_t n);

	void Run();

private:

	// Quadrature status flags
	enum e_quadrature_status_flags {
		STATUS_UNDERFLOW = 1 << 0, /**< encoder went below 0 **/
		STATUS_REVERSE = 1 << 1, /**< motor doing in reverse dir **/
		STATUS_OVERFLOW = 1 << 2, /**< encoder went above 2^32 **/
	};

	// commands
	// We just list the commands we want from the manual here.
	enum e_command {

		// simple
		CMD_DRIVE_FWD_1 = 0,
		CMD_DRIVE_REV_1 = 1,
		CMD_DRIVE_FWD_2 = 4,
		CMD_DRIVE_REV_2 = 5,

		CMD_DRIVE_FWD_MIX = 8,
		CMD_DRIVE_REV_MIX = 9,
		CMD_TURN_RIGHT = 10,
		CMD_TURN_LEFT = 11,

		// encoder commands
		CMD_READ_ENCODER_1 = 16,
		CMD_READ_ENCODER_2 = 17,
		CMD_RESET_ENCODERS = 20,

		// advanced motor control
		CMD_READ_SPEED_HIRES_1 = 30,
		CMD_READ_SPEED_HIRES_2 = 31,
		CMD_SIGNED_DUTYCYCLE_1 = 32,
		CMD_SIGNED_DUTYCYCLE_2 = 33,
	};

	uint8_t _address;
	uint16_t _pulsesPerRev;

	int _uart;
	fd_set _uart_set;
	struct timeval _uart_timeout;

	pthread_mutex_t _uart_mutex;

	/** poll structure for control packets */
	struct pollfd _controlPoll;

	/** actuator controls subscription */
	uORB::SubscriptionPollable<actuator_controls_s> _actuators;

	// private data
	int32_t _motor1EncoderCounts;
	int32_t _motor1Revolutions;
	int32_t _motor1Overflow;
	float _motor1Speed;

	int32_t _motor2EncoderCounts;
	int32_t _motor2Revolutions;
	int32_t _motor2Overflow;
	float _motor2Speed;

	// private methods
	uint16_t _sumBytes(uint8_t *buf, size_t n, uint16_t init = 0);
	int _sendUnsigned7Bit(e_command command, float data);
	int _sendSigned16Bit(e_command command, float data);
	int _sendNothing(e_command);

	/**
	 * Perform a round-trip write and read.
	 * @param cmd Command to send to the Roboclaw
	 * @param wbuff Write buffer. Must not contain command, address, or checksum. For most commands, this will be
	 *   one or two bytes. Can be null iff wbytes == 0.
	 * @param wbytes Number of bytes to write. Can be 0.
	 * @param rbuff Read buffer. Will be filled with the entire response, including a checksum if the Roboclaw sends
	 *   a checksum for this command.
	 * @param rbytes Maximum number of bytes to read.
	 * @param send_checksum If true, then the checksum will be calculated and sent to the Roboclaw.
	 *   This is an option because some Roboclaw commands expect no checksum.
	 * @param recv_checksum If true, then this function will calculate the checksum of the returned data and compare
	 *   it to the checksum received. If they are not equal, OR if fewer than 2 bytes were received, then an
	 *   error is returned.
	 *   If false, then this function will expect to read exactly one byte, 0xFF, and will return an error otherwise.
	 * @return If successful, then the number of bytes read from the Roboclaw is returned. If there is a timeout
	 *   reading from the Roboclaw, then 0 is returned. If there is an IO error, then a negative value is returned.
	 */
	int _transaction(e_command cmd, uint8_t *wbuff, size_t wbytes,
			 uint8_t *rbuff, size_t rbytes, bool send_checksum = true, bool recv_checksum = false);
};

// unit testing
int roboclawTest(const char *deviceName, uint8_t address,
		 uint16_t pulsesPerRev);

// vi:noet:smarttab:autoindent:ts=4:sw=4:tw=78
