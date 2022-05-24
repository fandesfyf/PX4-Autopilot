
#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/tasks.h>
#include <px4_platform_common/posix.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <math.h>

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <drivers/drv_hrt.h>
#include <string.h>
#include <systemlib/err.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uORB/topics/input_rc.h>
#include <poll.h>

#include <drivers/drv_pwm_output.h>
#include <uORB/uORB.h>
#include <uORB/topics/vehicle_acceleration.h>
#include <uORB/topics/servor_control.h>
#include <uORB/Subscription.hpp>
#include <string.h>
#include <modules/under_water_control/servor_control.hpp>

int servor_control_thread_start(int argc, char *argv[])
{
	uORB::Subscription servor_control_sub{ORB_ID(servor_control)};
	servor_control_s control_data;

	while (servor_control_thread_runnig){
		if(servor_control_sub.update(&control_data)){
			if (control_data.mask==1 || control_data.mask==3){
				set_servor_angle(control_data.leftangle,LEFT_CH);
			}
			if (control_data.mask==2 || control_data.mask==3){
				set_servor_angle(control_data.rightangle,RIGHT_CH);
			}
		}else{
			usleep(100);
		}
	}
	return 0;

}
int set_servor_angle(int angle, int servor_id)
{
	if (servor_id == RIGHT_CH) { //右侧
		up_pwm_servo_set(RIGHT_CH, (int)(RIGHT_PWM - RIGHT_range * angle / 90));

	} else if (servor_id == LEFT_CH) { //左侧
		up_pwm_servo_set(LEFT_CH, (int)(LEFT_PWM + LEFT_range * angle / 90));
	}
	return 0;
}
