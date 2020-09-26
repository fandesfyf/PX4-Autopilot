
px4_add_board(
	PLATFORM nuttx
	VENDOR nxp
	MODEL fmuk66-e
	LABEL socketcan
	TOOLCHAIN arm-none-eabi
	ARCHITECTURE cortex-m4
	ROMFSROOT px4fmu_common
	TESTING
	UAVCAN_INTERFACES 2
	SERIAL_PORTS
		GPS1:/dev/ttyS3
		TEL1:/dev/ttyS4
		TEL2:/dev/ttyS1
	DRIVERS
		adc/board_adc
		barometer # all available barometer drivers
		barometer/mpl3115a2
		batt_smbus
		camera_capture
		camera_trigger
		differential_pressure # all available differential pressure drivers
		distance_sensor # all available distance sensor drivers
		gps
		#heater
		#imu # all available imu drivers
		imu/bosch/bmi088
		imu/invensense/icm42688p
		irlock
		lights/blinkm
		lights/rgbled
		lights/rgbled_ncp5623c
		lights/rgbled_pwm
		magnetometer # all available magnetometer drivers
		mkblctrl
		#optical_flow # all available optical flow drivers
		optical_flow/px4flow
		#osd
		pca9685
		power_monitor/ina226
		#protocol_splitter
		pwm_out_sim
		pwm_out
		rc_input
		roboclaw
		safety_button
		tap_esc
		telemetry # all available telemetry drivers
		#test_ppm # NOT Portable YET
		tone_alarm
		#uavcannode_v1
	MODULES
		airspeed_selector
		attitude_estimator_q
		battery_status
		camera_feedback
		commander
		dataman
		ekf2
		events
		fw_att_control
		fw_pos_control_l1
		land_detector
		landing_target_estimator
		load_mon
		local_position_estimator
		logger
		mavlink
		mc_att_control
		mc_hover_thrust_estimator
		mc_pos_control
		mc_rate_control
		navigator
		rc_update
		rover_pos_control
		sensors
		sih
		temperature_compensation
		vmount
		vtol_att_control
	SYSTEMCMDS
		bl_update
		#dmesg
		dumpfile
		esc_calib
		#hardfault_log # Needs bbsrm
		i2cdetect
		led_control
		mixer
		motor_ramp
		motor_test
		mtd
		nshterm
		param
		perf
		pwm
		reboot
		reflect
		sd_bench
		system_time
		tests # tests and test runner
		top
		topic_listener
		tune_control
		usb_connected
		ver
		work_queue
	EXAMPLES
		fixedwing_control # Tutorial code from https://px4.io/dev/example_fixedwing_control
		hello
		hwtest # Hardware test
		#matlab_csv_serial
		px4_mavlink_debug # Tutorial code from http://dev.px4.io/en/debug/debug_values.html
		px4_simple_app # Tutorial code from http://dev.px4.io/en/apps/hello_sky.html
		rover_steering_control # Rover example app
		uuv_example_app
		work_item
	)
