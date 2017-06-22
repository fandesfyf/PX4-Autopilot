include(nuttx/px4_impl_nuttx)

px4_nuttx_configure(HWCLASS m4 CONFIG nsh ROMFS y ROMFSROOT px4fmu_common)

set(CMAKE_TOOLCHAIN_FILE ${PX4_SOURCE_DIR}/cmake/toolchains/Toolchain-arm-none-eabi.cmake)

set(config_uavcan_num_ifaces 1)

set(config_module_list
	#
	# Board support modules
	#
	drivers/device
	drivers/kinetis
	drivers/kinetis/adc
	drivers/kinetis/tone_alarm
	drivers/led
	drivers/px4fmu
	drivers/boards/nxphlite-v3
	drivers/rgbled
	drivers/fxos8700cq
	drivers/mpu6000
	drivers/mpu9250
	drivers/hmc5883
	drivers/ms5611
	drivers/mpl3115a2
	drivers/mb12xx
	drivers/srf02
	drivers/sf0x
	drivers/sf1xx
	drivers/ll40ls
	drivers/trone
	drivers/gps
	drivers/pwm_out_sim
	drivers/hott
	drivers/hott/hott_telemetry
	drivers/hott/hott_sensors
	drivers/blinkm
	drivers/airspeed
	drivers/ets_airspeed
	drivers/meas_airspeed
	drivers/frsky_telemetry
	modules/sensors
	drivers/mkblctrl
	drivers/px4flow
	drivers/oreoled
	drivers/vmount
# NOT Portable YET drivers/pwm_input
	drivers/camera_trigger
	drivers/bst
	drivers/snapdragon_rc_pwm
	drivers/lis3mdl
	drivers/bmp280
#No External SPI drivers/bma180
#No External SPI 	drivers/bmi160
	drivers/tap_esc

	#
	# System commands
	#
	systemcmds/bl_update
	systemcmds/mixer
	systemcmds/param
	systemcmds/perf
	systemcmds/pwm
	systemcmds/esc_calib
## Needs bbsrm 	systemcmds/hardfault_log
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
	systemcmds/usb_connected

	#
	# Testing
	#
	drivers/sf0x/sf0x_tests
### NOT Portable YET drivers/test_ppm
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
##NO CAN YET	modules/uavcan
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
	lib/conversion
	lib/DriverFramework/framework
	lib/ecl
	lib/external_lgpl
	lib/geo
	lib/geo_lookup
	lib/launchdetection
	lib/led
	lib/mathlib
	lib/mathlib/math/filter
	lib/rc
	lib/runway_takeoff
	lib/tailsitter_recovery
	lib/terrain_estimation
	lib/version

	#
	# Platform
	#
	platforms/common
	platforms/nuttx
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
##NO CAN YET	uavcan
##NO CAN YET	uavcan_stm32_driver
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
