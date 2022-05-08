#ifndef CUSTOM_CMD
#define CUSTOM_CMD
#include <modules/mavlink/mavlink_stream.h>
#include <uORB/SubscriptionInterval.hpp>
#include <mavlink_types.h>
#include <modules/mavlink/mavlink_receiver.h>

#include <uORB/uORB.h>
class MavlinkStreamCustomCMD: public MavlinkStream
{
public:
	const char *get_name() const
	{
		return MavlinkStreamCustomCMD::get_name_static();
	}

	static const char *get_name_static()
	{
		return "CUSTOM_CMD";
	}

	static uint16_t get_id_static()
	{
		return MAVLINK_MSG_ID_CUSTOM_CMD;
	}

	uint16_t get_id()
	{
		return get_id_static();
	}

	static MavlinkStream *new_instance(Mavlink *mavlink)
	{
		return  new   MavlinkStreamCustomCMD(mavlink);
	}

	unsigned get_size()
	{
		return MAVLINK_MSG_ID_CUSTOM_CMD_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES;
	}
private:

	explicit MavlinkStreamCustomCMD(Mavlink *mavlink) : MavlinkStream(mavlink) {}

	bool send() override //用于PX4真正发送的函数
	{

		// struct vehicle_zkrt_s _vehicle_zkrt;    //定义uorb消息结构体

		if (true) {

			//int sensor_sub_fd = orb_subscribe(ORB_ID(vehicle_zkrt));//订阅
			//orb_copy(ORB_ID(vehicle_zkrt), sensor_sub_fd, &_vehicle_zkrt);//获取消息数据

			// uORB::Subscription _zkrt_sub{ORB_ID(vehicle_zkrt)};//订阅
			// _zkrt_sub.copy(&_vehicle_zkrt);//获取消息数据

			char s[50] = "sdfaffff";

			mavlink_msg_custom_cmd_send(_mavlink->get_channel(), hrt_absolute_time(), is_offboard_mode,1.7, 2.8, 3.8, 4.4, 55.0,
						    s); //利用生成器生成的mavlink_msg_vehicle_zkrt.h头文件里面的这个函数将msg（mavlink结构体）封装成mavlink消息帧并发送；
			return true;
		}

		return false;
	}

};


#endif
