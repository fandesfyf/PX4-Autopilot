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
#include <uORB/topics/offboard_cmd.h>
#include <uORB/topics/under_water_control_status.h>

static bool thread_should_exit = false; /*Ddemon exit flag*///定义查看进程存在标志变量
static bool thread_running = false;  /*Daemon status flag*///定义查看进程运行标志变量

int uart_init(const char * uart_name);//串口初始化函数，形参为串口路径
int set_uart_baudrate(const int fd, unsigned int baud);//设置串口波特率函数
void usage(const char *reason);//进程提示函数
int uart_thread_main(int argc, char *argv[]);
