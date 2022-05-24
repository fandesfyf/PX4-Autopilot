#define RIGHT_CH 1
#define LEFT_CH 2
#define RIGHT_range 650//转90°需要的值
#define LEFT_range 650
#define RIGHT_PWM  1600 //右侧正常位置pwm1600,增大后转,950->90°,2250->-90°
#define LEFT_PWM  755 //左侧正常位置755,增大前转,1400->90°,500->-45°

static bool servor_control_thread_runnig = false;

int servor_control_thread_start(int argc, char *argv[]);
int set_servor_angle(int angle, int servor_id);
