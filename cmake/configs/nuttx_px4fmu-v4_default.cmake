include(nuttx/px4_impl_nuttx)

px4_nuttx_configure(HWCLASS m4 CONFIG nsh ROMFS y ROMFSROOT px4fmu_common)

set(CMAKE_TOOLCHAIN_FILE ${PX4_SOURCE_DIR}/cmake/toolchains/Toolchain-arm-none-eabi.cmake)

set(config_uavcan_num_ifaces 1)

set(config_module_list
	#
	# Board support modules
	#
	drivers/device
	drivers/stm32
	drivers/stm32/adc
	drivers/stm32/tone_alarm
	drivers/led
	drivers/px4fmu
	drivers/boards/px4fmu-v4
	drivers/rgbled
	drivers/mpu6000
	drivers/mpu9250
	drivers/hmc5883
	drivers/ms5611
	drivers/mb12xx
	drivers/srf02
	drivers/sf0x
	drivers/sf1xx
	drivers/ll40ls
	drivers/trone
	drivers/gps
	drivers/pwm_out_sim
	drivers/pca9685
	drivers/hott
	drivers/hott/hott_telemetry
	drivers/hott/hott_sensors
	drivers/blinkm
	drivers/airspeed
	drivers/ets_airspeed
	drivers/meas_airspeed
	drivers/ms5525_airspeed
	drivers/frsky_telemetry
	modules/sensors
	drivers/mkblctrl
	drivers/px4flow
	drivers/oreoled
	drivers/vmount
	drivers/pwm_input
	drivers/camera_trigger
	drivers/bst
	drivers/snapdragon_rc_pwm
	drivers/lis3mdl
	drivers/bmp280
	drivers/bma180
	drivers/bmi160
	drivers/bmi055
	drivers/bmm150
	drivers/tap_esc
	drivers/iridiumsbd

	#
	# System commands
	#
	systemcmds/bl_update
	systemcmds/led_control
	systemcmds/mixer
	systemcmds/param
	systemcmds/perf
	systemcmds/pwm
	systemcmds/esc_calib
	systemcmds/hardfault_log
	systemcmds/reboot
	systemcmds/topic_listener
	systemcmds/top
	systemcmds/config
	systemcmds/nshterm
	systemcmds/mtd
	systemcmds/dumpfile
	systemcmds/ver
	systemcmds/sd_bench
	systemcmds/motor_ramp

	#
	# Testing
	#
	drivers/sf0x/sf0x_tests
	drivers/test_ppm
	modules/commander/commander_tests
	modules/mc_pos_control/mc_pos_control_tests
	lib/controllib/controllib_test
	modules/mavlink/mavlink_tests
	modules/unit_test
	modules/uORB/uORB_tests
	systemcmds/tests

	#
	# General system control
	#
	modules/commander
	modules/events
	modules/load_mon
	modules/navigator
	modules/mavlink
	modules/gpio_led
	modules/uavcan
	modules/land_detector
	modules/camera_feedback

	#
	# Estimation modules (EKF/ SO3 / other filters)
	#
	modules/attitude_estimator_q
	modules/position_estimator_inav
	modules/ekf2
	modules/local_position_estimator

	#
	# Vehicle Control
	#
	modules/fw_att_control
	modules/fw_pos_control_l1
	modules/gnd_att_control
	modules/gnd_pos_control
	modules/mc_att_control
	modules/mc_pos_control
	modules/vtol_att_control

	#
	# Logging
	#
	modules/sdlog2
	modules/logger

	#
	# Library modules
	#
	modules/systemlib/param
	modules/systemlib
	modules/systemlib/mixer
	modules/uORB
	modules/dataman

	#
	# Libraries
	#
	lib/controllib
	lib/mathlib
	lib/mathlib/math/filter
	lib/rc
	lib/ecl
	lib/external_lgpl
	lib/geo
	lib/geo_lookup
	lib/conversion
	lib/launchdetection
	lib/led
	lib/terrain_estimation
	lib/runway_takeoff
	lib/tailsitter_recovery
	lib/version
	lib/DriverFramework/framework
	platforms/nuttx

	# had to add for cmake, not sure why wasn't in original config
	platforms/common
	platforms/nuttx/px4_layer

	#
	# OBC challenge
	#
	modules/bottle_drop

	#
	# Rover apps
	#
	examples/rover_steering_control

	#
	# Demo apps
	#
	#examples/math_demo
	# Tutorial code from
	# https://px4.io/dev/px4_simple_app
	examples/px4_simple_app

	# Tutorial code from
	# https://px4.io/dev/daemon
	#examples/px4_daemon_app

	# Tutorial code from
	# https://px4.io/dev/debug_values
	#examples/px4_mavlink_debug

	# Tutorial code from
	# https://px4.io/dev/example_fixedwing_control
	examples/fixedwing_control

	# Hardware test
	#examples/hwtest

	# EKF
	examples/ekf_att_pos_estimator
)

set(config_extra_builtin_cmds
	serdis
	sercon
	)

set(config_extra_libs
	uavcan
	uavcan_stm32_driver
	)

set(config_io_extra_libs
	)

add_custom_target(sercon)
set_target_properties(sercon PROPERTIES
	PRIORITY "SCHED_PRIORITY_DEFAULT"
	MAIN "sercon"
	STACK_MAIN "2048"
	COMPILE_FLAGS "-Os")

add_custom_target(serdis)
set_target_properties(serdis PROPERTIES
	PRIORITY "SCHED_PRIORITY_DEFAULT"
	MAIN "serdis"
	STACK_MAIN "2048"
	COMPILE_FLAGS "-Os")
