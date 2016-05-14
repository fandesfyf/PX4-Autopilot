#pragma once

#include "log_writer.h"
#include "array.h"
#include <px4.h>
#include <drivers/drv_hrt.h>
#include <uORB/Subscription.hpp>
#include <uORB/topics/vehicle_status.h>

extern "C" __EXPORT int logger_main(int argc, char *argv[]);

#define TRY_SUBSCRIBE_INTERVAL 1000*1000	// interval in microseconds at which we try to subscribe to a topic
// if we haven't succeeded before

namespace px4
{
namespace logger
{

struct LoggerSubscription {
	int fd[ORB_MULTI_MAX_INSTANCES];
	uint64_t time_tried_subscribe;	// captures the time at which we checked last time if this instance existed
	const orb_metadata *metadata = nullptr;

	LoggerSubscription() {}

	LoggerSubscription(int fd_, const orb_metadata *metadata_) :
		metadata(metadata_)
	{
		fd[0] = fd_;
		time_tried_subscribe = 0;

		for (int i = 1; i < ORB_MULTI_MAX_INSTANCES; i++) { fd[i] = -1; }
	}
};

class Logger
{
public:
	Logger(size_t buffer_size, unsigned log_interval, bool log_on_start);

	~Logger();

	int add_topic(const orb_metadata *topic);

	int add_topic(const char *name, unsigned interval);

	static int start(char *const *argv);

	static void usage(const char *reason);

	void status();

private:
	static void run_trampoline(int argc, char *argv[]);

	void run();

	int create_log_dir();

	static bool file_exist(const char *filename);

	int get_log_file_name(char *file_name, size_t file_name_size);

	void start_log();

	void stop_log();

	void write_formats();

	void write_parameters();

	bool copy_if_updated_multi(orb_id_t topic, int multi_instance, int *handle, void *buffer, uint64_t *time_last_checked);

	static constexpr size_t 	MAX_TOPICS_NUM = 128;
	static constexpr unsigned	MAX_NO_LOGFOLDER = 999;	/**< Maximum number of log dirs */
	static constexpr unsigned	MAX_NO_LOGFILE = 999;	/**< Maximum number of log files */
	static constexpr const char 		*LOG_ROOT = PX4_ROOTFSDIR"/fs/microsd/log";

	bool						_task_should_exit = true;
	uint8_t 					*_log_buffer;
	char 						_log_dir[64];
	uORB::Subscription<vehicle_status_s>	_vehicle_status_sub {ORB_ID(vehicle_status)};
	bool						_enabled = false;
	bool 						_log_on_start;
	Array<LoggerSubscription, MAX_TOPICS_NUM>	_subscriptions;
	LogWriter					_writer;
	uint32_t					_log_interval;
};

Logger *logger_ptr;
int		logger_task = -1;
pthread_t _writer_thread;

}
}
