#include "log_writer.h"
#include <fcntl.h>
#include <string.h>

namespace px4
{
namespace logger
{

LogWriter::LogWriter(uint8_t *buffer, size_t buffer_size) :
	_buffer(buffer),
	_buffer_size(buffer_size),
	_min_write_chunk(4096)
{
	pthread_mutex_init(&_mtx, nullptr);
	pthread_cond_init(&_cv, nullptr);
	/* allocate write performance counters */
	perf_write = perf_alloc(PC_ELAPSED, "sd write");
	perf_fsync = perf_alloc(PC_ELAPSED, "sd fsync");
}

void LogWriter::start_log(const char *filename)
{
	::strncpy(_filename, filename, sizeof(_filename));
	// Clear buffer and counters
	_head = 0;
	_count = 0;
	_total_written = 0;
	_should_run = true;
	notify();
}

void LogWriter::stop_log()
{
	_should_run = false;
	notify();
}

pthread_t LogWriter::thread_start()
{
	pthread_attr_t thr_attr;
	pthread_attr_init(&thr_attr);

	sched_param param;
	/* low priority, as this is expensive disk I/O */
	param.sched_priority = SCHED_PRIORITY_DEFAULT - 40;
	(void)pthread_attr_setschedparam(&thr_attr, &param);

	pthread_attr_setstacksize(&thr_attr, 1024);

	pthread_t thr;

	if (0 != pthread_create(&thr, &thr_attr, &LogWriter::run_helper, this)) {
		PX4_WARN("error creating logwriter thread");
	}

	pthread_attr_destroy(&thr_attr);

	return thr;
}

void LogWriter::thread_stop()
{
	// this will terminate the main loop of the writer thread
	_exit_thread = true;
	_should_run = false;
}

void *LogWriter::run_helper(void *context)
{
	px4_prctl(PR_SET_NAME, "log_writer", px4_getpid());

	reinterpret_cast<LogWriter *>(context)->run();
	return nullptr;
}

void LogWriter::run()
{
	// Wait for _should_run flag
	while (!_exit_thread) {
		bool start = false;
		pthread_mutex_lock(&_mtx);
		pthread_cond_wait(&_cv, &_mtx);
		start = _should_run;
		pthread_mutex_unlock(&_mtx);

		if (start) {
			break;
		}
	}

	while (!_exit_thread) {
		// Outer endless loop, start new file each time
		// _filename must be set before setting _should_run = true


		_fd = ::open(_filename, O_CREAT | O_WRONLY, PX4_O_MODE_666);

		if (_fd < 0) {
			PX4_WARN("can't open log file %s", _filename);
			_should_run = false;
			continue;
		}

		PX4_WARN("started, log file: %s", _filename);

		_should_run = true;

		int poll_count = 0;
		int written = 0;

		while (true) {
			size_t available = 0;
			void *read_ptr = nullptr;
			bool is_part = false;

			/* lock _buffer
			 * wait for sufficient data, cycle on notify()
			 */
			pthread_mutex_lock(&_mtx);

			while (true) {
				available = get_read_ptr(&read_ptr, &is_part);

				/* if sufficient data available or partial read or terminating, exit this wait loop */
				if ((available > _min_write_chunk) || is_part || !_should_run) {
					/* GOTO end of block */
					break;
				}

				/* wait for a call to notify()
				 * this call unlocks the mutex while waiting, and returns with the mutex locked
				 */
				pthread_cond_wait(&_cv, &_mtx);
			}

			pthread_mutex_unlock(&_mtx);

			if (available > 0) {
				perf_begin(perf_write);
				written = ::write(_fd, read_ptr, available);
				perf_end(perf_write);

				/* call fsync periodically to minimize potential loss of data */
				if (++poll_count >= 100) {
					perf_begin(perf_fsync);
					::fsync(_fd);
					perf_end(perf_fsync);
					poll_count = 0;
				}

				if (written < 0) {
					PX4_WARN("error writing log file");
					_should_run = false;
					/* GOTO end of block */
					break;
				}

				pthread_mutex_lock(&_mtx);
				/* subtract bytes written from number in _buffer (_count -= written) */
				mark_read(written);
				pthread_mutex_unlock(&_mtx);

				_total_written += written;
			}

			if (!_should_run && written == static_cast<int>(available) && !is_part) {
				// Stop only when all data written
				break;
			}
		}

		_head = 0;
		_count = 0;

		int res = ::close(_fd);

		if (res) {
			PX4_WARN("error closing log file");
		}

		PX4_WARN("stopped, bytes written: %zu", _total_written);
	}
}

bool LogWriter::write(void *ptr, size_t size)
{

	// Bytes available to write
	size_t available = _buffer_size - _count;

	if (size > available) {
		// buffer overflow
		return false;
	}

	size_t n = _buffer_size - _head;	// bytes to end of the buffer

	uint8_t *buffer_c = reinterpret_cast<uint8_t *>(ptr);

	if (size > n) {
		// Message goes over the end of the buffer
		memcpy(&(_buffer[_head]), buffer_c, n);
		_head = 0;

	} else {
		n = 0;
	}

	// now: n = bytes already written
	size_t p = size - n;	// number of bytes to write
	memcpy(&(_buffer[_head]), &(buffer_c[n]), p);
	_head = (_head + p) % _buffer_size;
	_count += size;
	return true;
}

size_t LogWriter::get_read_ptr(void **ptr, bool *is_part)
{
	// bytes available to read
	int read_ptr = _head - _count;

	if (read_ptr < 0) {
		read_ptr += _buffer_size;
		*ptr = &_buffer[read_ptr];
		*is_part = true;
		return _buffer_size - read_ptr;

	} else {
		*ptr = &_buffer[read_ptr];
		*is_part = false;
		return _count;
	}
}

}
}
