
px4_add_board(
	PLATFORM nuttx
	VENDOR px4
	MODEL fmu-v4
	LABEL rtps
	TOOLCHAIN arm-none-eabi
	ARCHITECTURE cortex-m4
	CONSTRAINED_MEMORY
	ROMFSROOT px4fmu_common
	TESTING
	UAVCAN_INTERFACES 1
	SERIAL_PORTS
		GPS1:/dev/ttyS3
		TEL1:/dev/ttyS1
		TEL2:/dev/ttyS2
	DRIVERS
		adc/board_adc
		barometer # all available barometer drivers
		batt_smbus
		camera_capture
		camera_trigger
		differential_pressure # all available differential pressure drivers
		distance_sensor # all available distance sensor drivers
		dshot
		gps
		heater
		#imu # all available imu drivers
		imu/adis16448
		imu/adis16477
		imu/adis16497
		imu/invensense/icm20602
		imu/invensense/icm20608g
		imu/invensense/icm40609d
		imu/invensense/mpu6500
		imu/invensense/mpu9250
		irlock
		lights/blinkm
		lights/rgbled
		lights/rgbled_ncp5623c
		magnetometer # all available magnetometer drivers
		mkblctrl
		optical_flow # all available optical flow drivers
		#osd
		pca9685
		protocol_splitter
		pwm_input
		pwm_out_sim
		pwm_out
		rc_input
		roboclaw
		safety_button
		tap_esc
		telemetry # all available telemetry drivers
		test_ppm
		tone_alarm
		uavcan
	MODULES
		airspeed_selector
		attitude_estimator_q
		battery_status
		camera_feedback
		commander
		dataman
		ekf2
		events
		flight_mode_manager
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
		micrortps_bridge
		navigator
		rc_update
		rover_pos_control
		sensors
		sih
		temperature_compensation
		uuv_att_control
		vmount
		vtol_att_control
	SYSTEMCMDS
		bl_update
		#dmesg
		dumpfile
		esc_calib
		hardfault_log
		i2cdetect
		led_control
		mft
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
