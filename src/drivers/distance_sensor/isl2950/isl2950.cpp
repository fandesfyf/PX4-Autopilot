/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
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
 * @file isl2950.cpp
 * @author Claudio Micheli <claudio@auterion.com>
 *
 * Driver for the Lanbao PSK-CM8JL65-CC5 distance sensor.
 * Make sure to disable MAVLINK messages (MAV_0_CONFIG PARAMETER)
 * on the serial port you connect the sensor,i.e TELEM1.
 *
 */

 #include <px4_config.h>
 #include <px4_getopt.h>
 #include <px4_workqueue.h>

 #include <sys/types.h>
 #include <stdint.h>
 #include <stdlib.h>
 #include <stdbool.h>
 #include <string.h>
 #include <poll.h>
 #include <stdio.h>
 #include <unistd.h>
 #include <termios.h>

 #include <perf/perf_counter.h>

 #include <drivers/drv_hrt.h>
 #include <drivers/drv_range_finder.h>
 #include <drivers/device/device.h>
 #include <drivers/device/ringbuffer.h>

 #include <uORB/uORB.h>
 #include <uORB/topics/distance_sensor.h>

 #include "isl2950_parser.h"

 /* Configuration Constants */

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif

#define ISL2950_TAKE_RANGE_REG		'd'

// designated serial port on Pixhawk (TELEM1)
#define ISL2950_DEFAULT_PORT		"/dev/ttyS1" // Its baudrate is 115200

// normal conversion wait time
#define ISL2950_CONVERSION_INTERVAL 50*1000UL/* 50ms */


 class ISL2950 : public cdev::CDev
 {
 public:

   // Constructor
   ISL2950(const char *port = ISL2950_DEFAULT_PORT, uint8_t rotation = distance_sensor_s::ROTATION_DOWNWARD_FACING);

   // Virtual destructor
   virtual ~ISL2950();

   virtual int  init();
   virtual int  ioctl(device::file_t *filp, int cmd, unsigned long arg);

    /**
  	* Diagnostics - print some basic information about the driver.
  	*/
  	void				print_info();

 private:

   char 				             _port[20];
   uint8_t                   _rotation;
   float				             _min_distance;
   float				             _max_distance;
   int         	             _conversion_interval;
   work_s				             _work{};
   ringbuffer::RingBuffer	  *_reports;
   int				               _fd;
   uint8_t			             _linebuf[25];
   uint8_t                   _cycle_counter;

   enum ISL2950_PARSE_STATE	 _parse_state;
   unsigned char             _frame_data[4];
   uint16_t                  _crc16;
   int                       _distance_mm;

   int				               _class_instance;
   int				               _orb_class_instance;

   orb_advert_t			         _distance_sensor_topic;

   perf_counter_t			       _sample_perf;
   perf_counter_t			       _comms_errors;

   /**
	* Initialise the automatic measurement state machine and start it.
	*
	* @note This function is called at open and error time.  It might make sense
	*       to make it more aggressive about resetting the bus in case of errors.
	*/
	 void				start();

	 /**
	 * Stop the automatic measurement state machine.
	 */
	 void				stop();

   /**
	 * Set the min and max distance thresholds.
  */
	 void				set_minimum_distance(float min);
	 void				set_maximum_distance(float max);
	 float			get_minimum_distance();
	 float			get_maximum_distance();

   /**
   * Perform a reading cycle; collect from the previous measurement
   * and start a new one.
   */
   void				cycle();
   int				collect();
   /**
   * Static trampoline from the workq context; because we don't have a
   * generic workq wrapper yet.
   *
   * @param arg		Instance pointer for the driver that is polling.
   */
   static void			cycle_trampoline(void *arg);

  };

  /*
   * Driver 'main' command.
   */
  extern "C" __EXPORT int isl2950_main(int argc, char *argv[]);

/**
* Method : Constructor
*
* @note This method initializes the class variables
*/

  ISL2950::ISL2950(const char *port, uint8_t rotation) :
	CDev(RANGE_FINDER0_DEVICE_PATH),
	_rotation(rotation),
	_min_distance(0.10f),
	_max_distance(9.0f),
	_conversion_interval(ISL2950_CONVERSION_INTERVAL),
	_reports(nullptr),
	_fd(-1),
  _cycle_counter(0),
	_parse_state(STATE0_WAITING_FRAME),
  _frame_data{START_FRAME_DIGIT1, START_FRAME_DIGIT2, 0, 0},
  _crc16(0),
  _distance_mm(-1),
	_class_instance(-1),
	_orb_class_instance(-1),
	_distance_sensor_topic(nullptr),
	_sample_perf(perf_alloc(PC_ELAPSED, "isl2950_read")),
	_comms_errors(perf_alloc(PC_COUNT, "isl2950_com_err"))
  {
	/* store port name */
	strncpy(_port, port, sizeof(_port));
	/* enforce null termination */
	_port[sizeof(_port) - 1] = '\0';

}

// Destructor
ISL2950::~ISL2950()
{
	/* make sure we are truly inactive */
	stop();

	/* free any existing reports */
	if (_reports != nullptr) {
		delete _reports;
	}

	if (_class_instance != -1) {
		unregister_class_devname(RANGE_FINDER_BASE_DEVICE_PATH, _class_instance);
	}

	perf_free(_sample_perf);
	perf_free(_comms_errors);
}

/**
* Method : init()
*
* This method setup the general driver for a range finder sensor.
*/

int
ISL2950::init()
{
 /* status */
int ret = 0;

do { /* create a scope to handle exit conditions using break */

  		/* do regular cdev init */
  		ret = CDev::init();

  		if (ret != OK) { break; }

  		/* allocate basic report buffers */
  		_reports = new ringbuffer::RingBuffer(2, sizeof(distance_sensor_s));

  		if (_reports == nullptr) {
  			PX4_ERR("alloc failed");
  			ret = -1;
  			break;
  		}
  		_class_instance = register_class_devname(RANGE_FINDER_BASE_DEVICE_PATH);

  		/* get a publish handle on the range finder topic */
  		struct distance_sensor_s ds_report = {};

  		_distance_sensor_topic = orb_advertise_multi(ORB_ID(distance_sensor), &ds_report,
  					 &_orb_class_instance, ORB_PRIO_HIGH);

  		if (_distance_sensor_topic == nullptr) {
  			PX4_ERR("failed to create distance_sensor object");
  		}

  	} while (0);

  	return ret;
}

void
ISL2950::set_minimum_distance(float min)
{
	_min_distance = min;
}

void
ISL2950::set_maximum_distance(float max)
{
	_max_distance = max;
}

float
ISL2950::get_minimum_distance()
{
	return _min_distance;
}

float
ISL2950::get_maximum_distance()
{
	return _max_distance;
}

int
ISL2950::ioctl(device::file_t *filp, int cmd, unsigned long arg)
{
	switch (cmd) {

	case SENSORIOCSPOLLRATE: {
			switch (arg) {

			/* zero would be bad */
			case 0:
				return -EINVAL;

			/* set default polling rate */
			case SENSOR_POLLRATE_DEFAULT: {
					start();
					return OK;
				}

			/* adjust to a legal polling interval in Hz */
			default: {

					/* convert hz to tick interval via microseconds */
					int ticks = USEC2TICK(1000000 / arg);

					/* check against maximum rate */
					if (ticks < USEC2TICK(_conversion_interval)) {
						return -EINVAL;
					}

						start();


					return OK;
				}
			}
		}

	default:
		/* give it to the superclass */
		return CDev::ioctl(filp, cmd, arg);
	}
}

int

/*
 * Method: collect()
 *
 * This method reads data from serial UART and places it into a buffer
*/
ISL2950::collect()
{
  int bytes_read = 0;
  int bytes_processed = 0;
  int i = 0;
  bool crc_valid = false;


  perf_begin(_sample_perf);


    /* read from the sensor (uart buffer) */
    bytes_read = ::read(_fd, &_linebuf[0], sizeof(_linebuf));


    if (bytes_read < 0) {
  		PX4_DEBUG("read err: %d \n", bytes_read);
  		perf_count(_comms_errors);
  		perf_end(_sample_perf);

    } else if (bytes_read > 0){
    //     printf("Bytes read: %d \n",bytes_read);
         i = bytes_read - 6 ;
         while ((i >=0) && (!crc_valid))
         {
           if (_linebuf[i] == START_FRAME_DIGIT1) {
             bytes_processed = i;
             while ((bytes_processed < bytes_read) && (!crc_valid))
             {
            //    printf("In the cycle, processing  byte %d, 0x%02X \n",bytes_processed, _linebuf[bytes_processed]);
                if (OK == isl2950_parser(_linebuf[bytes_processed],_frame_data, &_parse_state,&_crc16, &_distance_mm)){
                    crc_valid = true;
                }
                bytes_processed++;
             }
           _parse_state = STATE0_WAITING_FRAME;

         }
         // else {printf("Starting frame wrong. Index: %d value 0x%02X \n",i,_linebuf[i]);}
         i--;
        }

    }

  if (!crc_valid) {
		return -EAGAIN;
	}


  //printf("val (int): %d, raw: 0x%08X, valid: %s \n", _distance_mm, _frame_data, ((crc_valid) ? "OK" : "NO"));

	struct distance_sensor_s report;

	report.timestamp = hrt_absolute_time();
	report.type = distance_sensor_s::MAV_DISTANCE_SENSOR_LASER;
	report.orientation = _rotation;
	report.current_distance = _distance_mm/1000.0f;
	report.min_distance = get_minimum_distance();
	report.max_distance = get_maximum_distance();
	report.covariance = 0.0f;
	report.signal_quality = -1;
	/* TODO: set proper ID */
	report.id = 0;

	/* publish it */
	orb_publish(ORB_ID(distance_sensor), _distance_sensor_topic, &report);

	_reports->force(&report);

	/* notify anyone waiting for data */
	poll_notify(POLLIN);

	bytes_read = OK;

	perf_end(_sample_perf);

  /* ENABLE THIS IF YOU WANT TO PRINT OLD VALUES WHILE CRC CHECK IS WRONG
  if (!crc_valid) {
  		return -EAGAIN;
  	}
  else return OK; */
  return OK;

}

void
ISL2950::start()
{
  PX4_INFO("driver started");

	_reports->flush();

	/* schedule a cycle to start things */
	work_queue(HPWORK, &_work, (worker_t)&ISL2950::cycle_trampoline, this, 1);

}

void
ISL2950::stop()
{
	work_cancel(HPWORK, &_work);
}

void
ISL2950::cycle_trampoline(void *arg)
{
	ISL2950 *dev = static_cast<ISL2950 *>(arg);

	dev->cycle();
}

void
ISL2950::cycle()
{
  //PX4_DEBUG("ISL2950::cycle() - in the cycle");
	/* fds initialized? */
	if (_fd < 0) {
		/* open fd */
		_fd = ::open(_port,O_RDWR);

		if (_fd < 0) {
			PX4_ERR("open failed (%i)", errno);
			return;
		}

		struct termios uart_config;

		int termios_state;

		/* fill the struct for the new configuration */
		tcgetattr(_fd, &uart_config);

		/* clear ONLCR flag (which appends a CR for every LF) */
		uart_config.c_oflag &= ~ONLCR;

		/* no parity, one stop bit */
		uart_config.c_cflag &= ~(CSTOPB | PARENB);

		unsigned speed = B115200;

		/* set baud rate */
		if ((termios_state = cfsetispeed(&uart_config, speed)) < 0) {
			PX4_ERR("CFG: %d ISPD", termios_state);
		}

		if ((termios_state = cfsetospeed(&uart_config, speed)) < 0) {
			PX4_ERR("CFG: %d OSPD", termios_state);
		}

		if ((termios_state = tcsetattr(_fd, TCSANOW, &uart_config)) < 0) {
			PX4_ERR("baud %d ATTR", termios_state);
		}
	}

		/* perform collection */
		int collect_ret = collect();

		if (collect_ret == -EAGAIN) {
      _cycle_counter++;
			/* We are missing bytes to complete the packet, re-cycle at 1ms */
		//	work_queue(HPWORK,&_work,(worker_t)&ISL2950::cycle_trampoline,this,USEC2TICK(1000LL));
		//	return;
		}


	/* schedule a fresh cycle call when a complete packet has been received */
	//work_queue(HPWORK,&_work,(worker_t)&ISL2950::cycle_trampoline,this,USEC2TICK(_conversion_interval - _cycle_counter * 1000LL));
  work_queue(HPWORK,&_work,(worker_t)&ISL2950::cycle_trampoline,this,USEC2TICK(_conversion_interval));
  _cycle_counter = 0;
}

void
ISL2950::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	_reports->print_info("report queue");
}

/**
 * Local functions in support of the shell command.
 */
namespace isl2950
{

ISL2950	*g_dev;

int	start(const char *port, uint8_t rotation);
int	stop();
int	test();
int	reset();
int	info();

/**
 * Start the driver.
 */
int
start(const char *port, uint8_t rotation)
{
	int fd;

	if (g_dev != nullptr) {
		PX4_WARN("already started");
		return -1;
	}

	/* create the driver */
	g_dev = new ISL2950(port, rotation);

	if (g_dev == nullptr) {
		goto fail;
	}

	if (OK != g_dev->init()) {
		goto fail;
	}

	/* set the poll rate to default, starts automatic data collection */
	fd = open(RANGE_FINDER0_DEVICE_PATH, 0);

	if (fd < 0) {
		PX4_ERR("device open fail (%i)", errno);
		goto fail;
	}

	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0) {
    PX4_ERR("failed to set baudrate %d", B115200);
		goto fail;
	}
  PX4_DEBUG("isl2950::start() succeeded");
	return 0;

fail:
  PX4_DEBUG("isl2950::start() failed");
	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;
	}

	return -1;
}

/**
 * Stop the driver
 */
int stop()
{
	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;

	} else {
		return -1;
	}

	return 0;
}

/**
 * Perform some basic functional tests on the driver;
 * make sure we can collect data from the sensor in polled
 * and automatic modes.
 */
int
test()
{
	struct distance_sensor_s report;
	ssize_t sz;

	int fd = open(RANGE_FINDER0_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		PX4_ERR("%s open failed (try 'isl2950 start' if the driver is not running", RANGE_FINDER0_DEVICE_PATH);
		return -1;
	}

	/* do a simple demand read */
	sz = read(fd, &report, sizeof(report));

	if (sz != sizeof(report)) {
		PX4_ERR("immediate read failed");
		return -1;
	}

	print_message(report);

	/* start the sensor polling at 2 Hz rate */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 2)) {
		PX4_ERR("failed to set 2Hz poll rate");
		return -1;
	}

	/* read the sensor 5x and report each value */
	for (unsigned i = 0; i < 5; i++) {
		struct pollfd fds;

		/* wait for data to be ready */
		fds.fd = fd;
		fds.events = POLLIN;
		int ret = poll(&fds, 1, 2000);

		if (ret != 1) {
			PX4_ERR("timed out");
			break;
		}

		/* now go get it */
		sz = read(fd, &report, sizeof(report));

		if (sz != sizeof(report)) {
			PX4_ERR("read failed: got %zi vs exp. %zu", sz, sizeof(report));
			break;
		}

		print_message(report);
	}

	/* reset the sensor polling to the default rate */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT)) {
		PX4_ERR("ioctl SENSORIOCSPOLLRATE failed");
		return -1;
	}

	return 0;
}

/**
 * Reset the driver.
 */
int
reset()
{
	int fd = open(RANGE_FINDER0_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		PX4_ERR("open failed (%i)", errno);
		return -1;
	}

	if (ioctl(fd, SENSORIOCRESET, 0) < 0) {
		PX4_ERR("driver reset failed");
		return -1;
	}

	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0) {
		PX4_ERR("driver poll restart failed");
		return -1;
	}

	return 0;
}

/**
 * Print a little info about the driver.
 */
int
info()
{
	if (g_dev == nullptr) {
		PX4_ERR("driver not running");
		return -1;
	}

	printf("state @ %p\n", g_dev);
	g_dev->print_info();

	return 0;
}

} // namespace

int
isl2950_main(int argc, char *argv[])
{
	uint8_t rotation = distance_sensor_s::ROTATION_DOWNWARD_FACING;
	const char *device_path = ISL2950_DEFAULT_PORT;
	int ch;
	int myoptind = 1;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "R:d:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'R':
			rotation = (uint8_t)atoi(myoptarg);
			break;

		case 'd':
			device_path = myoptarg;
			break;

		default:
			PX4_WARN("Unknown option!");
			return -1;
		}
	}

	if (myoptind >= argc) {
		goto out_error;
	}

	/*
	 * Start/load the driver.
	 */
	if (!strcmp(argv[myoptind], "start")) {
		return isl2950::start(device_path, rotation);
	}

	/*
	 * Stop the driver
	 */
	if (!strcmp(argv[myoptind], "stop")) {
		return isl2950::stop();
	}

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(argv[myoptind], "test")) {
		return isl2950::test();
	}

	/*
	 * Reset the driver.
	 */
	if (!strcmp(argv[myoptind], "reset")) {
		return isl2950::reset();
	}

	/*
	 * Print driver information.
	 */
	if (!strcmp(argv[myoptind], "info") || !strcmp(argv[myoptind], "status")) {
		return isl2950::info();
	}

out_error:
	PX4_ERR("unrecognized command, try 'start', 'test', 'reset' or 'info'");
	return -1;
}
