
#include <unit_test.h>

#include <time.h>
#include <stdlib.h>

#include <drivers/drv_hrt.h>
#include <perf/perf_counter.h>
#include <px4_config.h>
#include <px4_micro_hal.h>

#ifdef __PX4_NUTTX
#include <nuttx/irq.h>
static irqstate_t flags;
#endif

void lock()
{
#ifdef __PX4_NUTTX
	flags = px4_enter_critical_section();
#endif
}

void unlock()
{
#ifdef __PX4_NUTTX
	px4_leave_critical_section(flags);
#endif
}

#define PERF(name, op, count) do { \
		usleep(1000); \
		reset(); \
		perf_counter_t p = perf_alloc(PC_ELAPSED, name); \
		for (int i = 0; i < count; i++) { \
			lock(); \
			perf_begin(p); \
			op; \
			perf_end(p); \
			unlock(); \
			reset(); \
		} \
		perf_print_counter(p); \
		perf_free(p); \
	} while (0)

class MicroBench : public UnitTest
{
public:
	virtual bool run_tests();

private:
	bool time_single_prevision_float();
	bool time_single_prevision_float_trig();

	bool time_double_prevision_float();
	bool time_double_prevision_float_trig();

	bool time_8bit_integers();
	bool time_16bit_integers();
	bool time_32bit_integers();
	bool time_64bit_integers();

	bool time_px4_hrt();

	void reset();


	volatile float f32;
	volatile float f32_out;

	volatile double f64;
	volatile double f64_out;

	volatile uint8_t i_8;
	volatile uint8_t i_8_out;

	volatile uint16_t i_16;
	volatile uint16_t i_16_out;

	volatile uint32_t i_32;
	volatile uint32_t i_32_out;

	volatile int64_t i_64;
	volatile int64_t i_64_out;

	volatile uint64_t u_64;
	volatile uint64_t u_64_out;
};

bool MicroBench::run_tests()
{
	ut_run_test(time_single_prevision_float);
	ut_run_test(time_single_prevision_float_trig);
	ut_run_test(time_double_prevision_float);
	ut_run_test(time_double_prevision_float_trig);
	ut_run_test(time_8bit_integers);
	ut_run_test(time_16bit_integers);
	ut_run_test(time_32bit_integers);
	ut_run_test(time_64bit_integers);
	ut_run_test(time_px4_hrt);

	return (_tests_failed == 0);
}

template<typename T>
T random(T min, T max)
{
	const T scale = rand() / (T) RAND_MAX; /* [0, 1.0] */
	return min + scale * (max - min);      /* [min, max] */
}

void MicroBench::reset()
{
	srand(time(nullptr));

	// initialize with random data
	f32 = random(-2.0f * M_PI, 2.0f * M_PI);		// somewhat representative range for angles in radians
	f32_out = random(-2.0f * M_PI, 2.0f * M_PI);

	f64 = random(-2.0 * M_PI, 2.0 * M_PI);
	f64_out = random(-2.0 * M_PI, 2.0 * M_PI);

	i_8 = rand();
	i_8_out = rand();

	i_16 = rand();
	i_16_out = rand();

	i_32 = rand();
	i_32_out = rand();

	i_64 = rand();
	i_64_out = rand();

	u_64 = rand();
	u_64_out = rand();
}

ut_declare_test_c(test_microbench, MicroBench)

bool MicroBench::time_single_prevision_float()
{
	PERF("float add", f32_out += f32, 1000);
	PERF("float sub", f32_out -= f32, 1000);
	PERF("float mul", f32_out *= f32, 1000);
	PERF("float div", f32_out /= f32, 1000);
	PERF("float sqrt", f32_out = sqrtf(f32), 1000);

	return true;
}

bool MicroBench::time_single_prevision_float_trig()
{
	PERF("sinf()", f32_out = sinf(f32), 1000);
	PERF("cosf()", f32_out = cosf(f32), 1000);
	PERF("tanf()", f32_out = tanf(f32), 1000);

	PERF("acosf()", f32_out = acosf(f32), 1000);
	PERF("asinf()", f32_out = asinf(f32), 1000);
	PERF("atan2f()", f32_out = atan2f(f32, 2.0f * f32), 1000);

	return true;
}

bool MicroBench::time_double_prevision_float()
{
	PERF("double add", f64_out += f64, 1000);
	PERF("double sub", f64_out -= f64, 1000);
	PERF("double mul", f64_out *= f64, 1000);
	PERF("double div", f64_out /= f64, 1000);
	PERF("double sqrt", f64_out = sqrt(f64), 1000);

	return true;
}

bool MicroBench::time_double_prevision_float_trig()
{
	PERF("sin()", f64_out = sin(f64), 1000);
	PERF("cos()", f64_out = cos(f64), 1000);
	PERF("tan()", f64_out = tan(f64), 1000);

	PERF("acos()", f64_out = acos(f64 * 0.5), 1000);
	PERF("asin()", f64_out = asin(f64 * 0.6), 1000);
	PERF("atan2()", f64_out = atan2(f64 * 0.7, f64 * 0.8), 1000);

	PERF("sqrt()", f64_out = sqrt(f64), 1000);

	return true;
}


bool MicroBench::time_8bit_integers()
{
	PERF("int8 add", i_8_out += i_8, 1000);
	PERF("int8 sub", i_8_out -= i_8, 1000);
	PERF("int8 mul", i_8_out *= i_8, 1000);
	PERF("int8 div", i_8_out /= i_8, 1000);

	return true;
}

bool MicroBench::time_16bit_integers()
{
	PERF("int16 add", i_16_out += i_16, 1000);
	PERF("int16 sub", i_16_out -= i_16, 1000);
	PERF("int16 mul", i_16_out *= i_16, 1000);
	PERF("int16 div", i_16_out /= i_16, 1000);

	return true;
}

bool MicroBench::time_32bit_integers()
{
	PERF("int32 add", i_32_out += i_32, 1000);
	PERF("int32 sub", i_32_out -= i_32, 1000);
	PERF("int32 mul", i_32_out *= i_32, 1000);
	PERF("int32 div", i_32_out /= i_32, 1000);

	return true;
}

bool MicroBench::time_64bit_integers()
{
	PERF("int64 add", i_64_out += i_64, 1000);
	PERF("int64 sub", i_64_out -= i_64, 1000);
	PERF("int64 mul", i_64_out *= i_64, 1000);
	PERF("int64 div", i_64_out /= i_64, 1000);

	return true;
}

bool MicroBench::time_px4_hrt()
{
	PERF("hrt_absolute_time()", u_64_out = hrt_absolute_time(), 1000);
	PERF("hrt_elapsed_time()", u_64_out = hrt_elapsed_time(&u_64), 1000);

	return true;
}
