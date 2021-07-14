
px4_add_board(
	PLATFORM nuttx
	VENDOR atl
	MODEL mantis-edu
	LABEL default
	TOOLCHAIN arm-none-eabi
	ARCHITECTURE cortex-m7
	ROMFSROOT px4fmu_common
	DRIVERS
		adc/board_adc
		barometer/maiertek/mpc2520
		camera_capture
		gps
		#heater
		imu/invensense/icm20602
		#lights/rgbled_pwm
		magnetometer/isentek/ist8310
		tap_esc
	MODULES
		battery_status
		camera_feedback
		commander
		dataman
		ekf2
		events
		flight_mode_manager
		gyro_calibration
		gyro_fft
		land_detector
		load_mon
		logger
		mavlink
		mc_att_control
		mc_hover_thrust_estimator
		mc_pos_control
		mc_rate_control
		#micrortps_bridge
		navigator
		rc_update
		sensors
		sih
		vmount
	SYSTEMCMDS
		bl_update
		dmesg
		dumpfile
		esc_calib
		hardfault_log
		i2cdetect
		led_control
		mixer
		motor_ramp
		motor_test
		nshterm
		param
		perf
		reboot
		reflect
		sd_bench
		serial_test
		shutdown
		system_time
		top
		topic_listener
		tune_control
		uorb
		usb_connected
		ver
		work_queue
	)
