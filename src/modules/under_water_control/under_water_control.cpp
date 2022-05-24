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
#include <uORB/topics/vehicle_attitude.h>
#include <string.h>
#include <modules/under_water_control/uartsensor.hpp>
#include <modules/under_water_control/servor_control.hpp>

extern "C" __EXPORT int under_water_control_main(int argc, char *argv[]);

static int daemon_task;//定义进程变量
static int servor_task;//定义进程变量

int under_water_control_main(int argc, char *argv[])
{


	uint16_t pwm = 0;
	PX4_INFO("Hello!hhhh%s", argv[0]);

	if (argc < 2) {

		usage("missing command");
	}

	if (!strcmp(argv[1], "pwm")) {
		if (argc == 4) {
			PX4_INFO("set pwm%d", pwm);
			up_pwm_servo_set((uint16_t)atoll(argv[2]), (uint16_t)atoll(argv[3]));

		} else {
			pwm = (uint16_t)atoll(argv[2]);
			PX4_INFO("set pwm%d", pwm);
			up_pwm_servo_set(3, pwm);
		}

	} else if (!strcmp(argv[1], "angle")) {
		if (argc == 4) {
			set_servor_angle((float)atoll(argv[2]), LEFT_CH);
			set_servor_angle((float)atoll(argv[3]), RIGHT_CH);

		} else { set_servor_angle((float)atoll(argv[2]), RIGHT_CH); }

	} else {
		up_pwm_servo_set(RIGHT_CH, RIGHT_PWM);
		up_pwm_servo_set(LEFT_CH, LEFT_PWM);
	}

	//输入为start
	if (!strcmp(argv[1], "start")) {
		if (thread_running) {//进程在运行
			warnx("already running\n");//打印提示已经在运行
			return 0;//跳出代码
		}


		//如果是第一次运行
		thread_should_exit = false;
		//建立名为serv_sys_uart进程SCHED_PRIORITY_MAX - 55,
		daemon_task = px4_task_spawn_cmd("uart_sensors_thread",
						 SCHED_DEFAULT,
						 SCHED_PRIORITY_DEFAULT - 50,
						 2500,
						 uart_thread_main,
						 (argv) ? (char *const *)&argv[2] : (char *const *)NULL);  //正常命令形式为serv_sys_uart start /dev/ttyS2

		//启动舵机控制线程
		if (!servor_control_thread_runnig) {
			servor_control_thread_runnig = true;
			servor_task = px4_task_spawn_cmd("servor_control_thread",
							 SCHED_DEFAULT,
							 SCHED_PRIORITY_DEFAULT,
							 1000,
							 servor_control_thread_start,
							 (argv) ? (char *const *)&argv[2] : (char *const *)NULL);
		}

		return 0;//跳出代码
	}

	//如果是stop
	if (!strcmp(argv[1], "stop")) {
		PX4_INFO("to stop ");
		thread_should_exit = true;//进程标志变量置true
		servor_control_thread_runnig = false;
		return 0;
	}

	//如果是status
	if (!strcmp(argv[1], "status")) {
		if (thread_running) {
			warnx("running");
			return 0;

		} else {
			warnx("stopped");
			return 1;
		}

		return 0;
	}

	//若果是其他，则打印不支持该类型
	usage("unrecognized command");
	return 1;


}
