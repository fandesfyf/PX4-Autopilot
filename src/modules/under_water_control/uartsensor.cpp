
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
//设置串口波特率函数
int set_uart_baudrate(const int fd, unsigned int baud)//配置不全，仿照GPS配置尝试
{
	int speed;

	//选择波特率
	switch (baud) {
	case 9600:   speed = B9600;   break;

	case 19200:  speed = B19200;  break;

	case 38400:  speed = B38400;  break;

	case 57600:  speed = B57600;  break;

	case 115200: speed = B115200; break;

	default:
		warnx("ERR: baudrate: %d\n", baud);
		return -EINVAL;
	}

	//实例化termios结构体，命名为uart_config
	struct termios uart_config;

	int termios_state;
	/* fill the struct for the new configuration */
	tcgetattr(fd, &uart_config);// 获取终端参数
	/* clear ONLCR flag (which appends a CR for every LF) */
	uart_config.c_oflag &= ~ONLCR;// 将NL转换成CR(回车)-NL后输出。
	/* no parity, one stop bit */
	uart_config.c_cflag &= ~(CSTOPB | PARENB);

	/* set baud rate */
	if ((termios_state = cfsetispeed(&uart_config, speed)) < 0) {
		warnx("ERR: %d (cfsetispeed)\n", termios_state);
		return false;
	}

	if ((termios_state = cfsetospeed(&uart_config, speed)) < 0) {
		warnx("ERR: %d (cfsetospeed)\n", termios_state);
		return false;
	}

	// 设置与终端相关的参数，TCSANOW 立即改变参数
	if ((termios_state = tcsetattr(fd, TCSANOW, &uart_config)) < 0) {
		warnx("ERR: %d (tcsetattr)\n", termios_state);
		return false;
	}

	return true;
}


//串口初始化函数，传入形参为"/dev/ttyS2"
int uart_init(const char *uart_name)
{
	int serial_fd = open(uart_name, O_RDWR | O_NOCTTY |
			     O_NONBLOCK); //调用Nuttx系统的open函数，形参为串口文件配置模式，可读写，

	/*Linux中，万物皆文件，打开串口设备和打开普通文件一样，使用的是open（）系统调用*/
	// 选项 O_NOCTTY 表示不能把本串口当成控制终端，否则用户的键盘输入信息将影响程序的执行
	if (serial_fd < 0) {
		err(1, "failed to open port: %s", uart_name);
		return false;
	}

	return serial_fd;
}
int uart_thread_main(int argc, char *argv[])
{
//正常命令形式为serv_sys_uart start /dev/ttyS2
	if (argc < 2) {
		errx(1, "need a serial port name as argument");
		usage("eg:");
	}

	/*将/dev/ttyS2赋值给argv[1],进而赋值给uart_name*/
	const char *uart_name = argv[1];
	warnx("opening port %s", uart_name);

	/*
	 * TELEM1 : /dev/ttyS1
	 * TELEM2 : /dev/ttyS2
	 * GPS    : /dev/ttyS3
	 * NSH    : /dev/ttyS5
	 * SERIAL4: /dev/ttyS6
	 * N/A    : /dev/ttyS4
	 * IO DEBUG (RX only):/dev/ttyS0
	 */

	/*配置串口*/
	int serv_uart = uart_init(uart_name);//初始化串口路径-

	if (false == serv_uart) { return -1; }

	if (false == set_uart_baudrate(serv_uart, 57600)) { //设置串口波特率为57600
		printf("set_uart_baudrate is failed\n");
		return -1;
	}

	PX4_INFO("under water thread start\n");
	/*进程标志变量*/
	thread_running = true;//进程正在运行


	/*定义接收话题变量*/
	uint8_t data;
	uint8_t buffer[25];//串口接收缓冲数组
	uint8_t b_point = 0;
	/*定义串口事件阻塞结构体及变量*/
	// px4_pollfd_struct_t fds[] = {
	// 	{ .fd = serv_uart,   .events = POLLIN },
	// };
	bool reading = false;

	char temp_buf[10];
	uint8_t temp_p=0;
	uint8_t get_res_count=0;
	double result[5];

	struct under_water_control_status_s uwc_status;//发送出去的传感器数据
	memset(&uwc_status, 0, sizeof(uwc_status));
	orb_advert_t uwc_status_pub = orb_advertise(ORB_ID(under_water_control_status), &uwc_status);


	while (!thread_should_exit) {

		//if((cur_time-last_time)<100000) continue;//时间少于100000微秒，即0.1s
		read(serv_uart, &data, 1);
		if (data == '>') {
			memset(temp_buf,'\0',sizeof(temp_buf));//清空值
			memset(buffer,'\0',sizeof(buffer));//清空值
			b_point = 0;
			buffer[b_point] = data;
			reading = true;
			temp_p=0;
			get_res_count=0;
		} else if (reading) {

			if (data=='|' || data=='<'){
				temp_buf[temp_p]='\0';
				result[get_res_count]=atof(temp_buf);
				get_res_count++;
				temp_p=0;
			}else{
				temp_buf[temp_p]=data;
				temp_p++;
			}


			b_point ++;
			buffer[b_point] = data;
			if (data == '<') {

				// PX4_INFO("read end %s\n%f %f %f", buffer,(double)result[0],(double)result[1],(double)result[2]);
				reading=false;
				uwc_status.timestamp= hrt_absolute_time();
				uwc_status.temperature=result[0];
				uwc_status.humidity=result[1];
				uwc_status.pressure=result[2];
				orb_publish(ORB_ID(under_water_control_status), uwc_status_pub, &uwc_status);
			}
		}
		usleep(50000);
	}

	//如果标志位置flase应该退出进程
	PX4_WARN("under water thread exiting");
	thread_running = false;
	close(serv_uart);
	fflush(stdout);
	return 0;
}
//进程提示函数，用来提示可输入的操作
void usage(const char *reason)
{
	if (reason) {
		fprintf(stderr, "%s\n", reason);
	}

	fprintf(stderr, "usage: serv_sys_uart {start|stop|status} [param]\n\n");
	exit(1);
}
