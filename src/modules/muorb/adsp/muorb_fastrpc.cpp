/****************************************************************************
 *
 * Copyright (C) 2015 Mark Charlebois. All rights reserved.
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
#include "muorb_fastrpc.h"
#include "qurt.h"
#include "uORBFastRpcChannel.hpp"
#include "uORBManager.hpp"

#include <px4_middleware.h>
#include <px4_tasks.h>
#include <px4_posix.h>
#include <map>
#include <string>
#include "px4_log.h"
#include "uORB/topics/sensor_combined.h"
#include "uORB.h"

#define _ENABLE_MUORB 1

__BEGIN_DECLS

int dspal_main(int argc, const char *argv[]);

__END_DECLS


int muorb_fastrpc_orb_initialize()
{
	int rc = 0;
	PX4_WARN("Before calling dspal_entry() method...");
	// registere the fastrpc muorb with uORBManager.
	uORB::Manager::get_instance()->set_uorb_communicator(uORB::FastRpcChannel::GetInstance());
	const char *argv[2] = { "dspal", "start" };
	int argc = 2;
	dspal_main(argc, argv);
	PX4_WARN("After calling dspal_entry");
	return rc;
}

int muorb_fastrpc_add_subscriber(const char *name)
{
	int rc = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	channel->AddRemoteSubscriber(name);
	uORBCommunicator::IChannelRxHandler *rxHandler = channel->GetRxHandler();

	if (rxHandler != nullptr) {
		rc = rxHandler->process_add_subscription(name, 0);

		if (rc != OK) {
			channel->RemoveRemoteSubscriber(name);
		}

	} else {
		rc = -1;
	}

	return rc;
}

int muorb_fastrpc_remove_subscriber(const char *name)
{
	int rc = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	channel->RemoveRemoteSubscriber(name);
	uORBCommunicator::IChannelRxHandler *rxHandler = channel->GetRxHandler();

	if (rxHandler != nullptr) {
		rc = rxHandler->process_remove_subscription(name);

	} else {
		rc = -1;
	}

	return rc;

}

int muorb_fastrpc_send_topic_data(const char *name, const uint8_t *data, int data_len_in_bytes)
{
	int rc = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	uORBCommunicator::IChannelRxHandler *rxHandler = channel->GetRxHandler();

	if (rxHandler != nullptr) {
		rc = rxHandler->process_received_message(name, data_len_in_bytes, (uint8_t *)data);

	} else {
		rc = -1;
	}

	return rc;
}

int muorb_fastrpc_is_subscriber_present(const char *topic_name, int *status)
{
	int rc = 0;
	int32_t local_status = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	rc = channel->is_subscriber_present(topic_name, &local_status);

	if (rc == 0) {
		*status = (int)local_status;
	}

	return rc;
}

int muorb_fastrpc_receive_msg(int *msg_type, char *topic_name, int topic_name_len, uint8_t *data, int data_len_in_bytes,
			      int *bytes_returned)
{
	int rc = 0;
	int32_t local_msg_type = 0;
	int32_t local_bytes_returned = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	rc = channel->get_data(&local_msg_type, topic_name, topic_name_len, data, data_len_in_bytes, &local_bytes_returned);
	*msg_type = (int)local_msg_type;
	*bytes_returned = (int)local_bytes_returned;
	return rc;
}

int muorb_fastrpc_unblock_recieve_msg(void)
{
	int rc = 0;
	uORB::FastRpcChannel *channel = uORB::FastRpcChannel::GetInstance();
	rc = channel->unblock_get_data_method();
	return rc;
}
