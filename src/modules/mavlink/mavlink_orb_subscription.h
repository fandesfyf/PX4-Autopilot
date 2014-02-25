/*
 * mavlink_orb_subscription.h
 *
 *  Created on: 23.02.2014
 *      Author: ton
 */

#ifndef MAVLINK_ORB_SUBSCRIPTION_H_
#define MAVLINK_ORB_SUBSCRIPTION_H_

#include <systemlib/uthash/utlist.h>
#include <drivers/drv_hrt.h>

class MavlinkOrbSubscription {
public:
	MavlinkOrbSubscription(const struct orb_metadata *meta, size_t size);
	~MavlinkOrbSubscription();

	bool update(const hrt_abstime t);

	const struct orb_metadata *topic;
	int fd;
	void *data;
	hrt_abstime last_update;
	MavlinkOrbSubscription *next;
};


#endif /* MAVLINK_ORB_SUBSCRIPTION_H_ */
