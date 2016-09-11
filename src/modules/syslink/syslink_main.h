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

#include <stdint.h>

#include <systemlib/battery.h>

#include <drivers/device/device.h>
#include <drivers/device/ringbuffer.h>

#include <uORB/uORB.h>

#include "syslink.h"
#include "crtp.h"

typedef enum {
	BAT_DISCHARGING = 0,
	BAT_CHARGING = 1,
	BAT_CHARGED = 2
} battery_state;


class SyslinkBridge;

class Syslink
{
public:
	Syslink();

	int start();

	// TODO: These should wait for an ACK
	int set_datarate(uint8_t datarate);
	int set_channel(uint8_t channel);
	int set_address(uint64_t addr);

private:

	friend class SyslinkBridge;

	int open_serial(const char *dev);

	void handle_message(syslink_message_t *msg);
	void handle_raw(syslink_message_t *sys);

	// Handles other types of messages that we don't really care about, but
	// will be maintained with the bare minimum implementation for supporting
	// other crazyflie libraries
	void handle_raw_other(syslink_message_t *sys);

	int send_bytes(const void *data, size_t len);

	// Checksums and transmits a syslink message
	int send_message(syslink_message_t *msg);

	int send_queued_raw_message();


	int _syslink_task;
	bool _task_running;

	int _fd;

	// Stores data that was needs to be written as a raw message
	ringbuffer::RingBuffer _writebuffer;
	SyslinkBridge *_bridge;

	orb_advert_t _battery_pub;
	orb_advert_t _rc_pub;
	orb_advert_t _cmd_pub;

	struct battery_status_s _battery_status;

	Battery _battery;

	int32_t _rssi;
	battery_state _bstate;

	static int task_main_trampoline(int argc, char *argv[]);

	void task_main();

};


class SyslinkBridge : public device::CDev
{

public:
	SyslinkBridge(Syslink *link);
	virtual ~SyslinkBridge();

	virtual int	init();

	virtual ssize_t	read(struct file *filp, char *buffer, size_t buflen);
	virtual ssize_t	write(struct file *filp, const char *buffer, size_t buflen);
	virtual int	ioctl(struct file *filp, int cmd, unsigned long arg);

	// Makes the message available for reading to processes reading from the bridge
	void pipe_message(crtp_message_t *msg);

protected:

	virtual pollevent_t poll_state(struct file *filp);

private:

	Syslink *_link;

	// Stores data that was received from syslink but not yet read by another driver
	ringbuffer::RingBuffer _readbuffer;


};
