
px4_add_board(
	VENDOR beaglebone
	MODEL blue
	LABEL default
	PLATFORM posix
	ARCHITECTURE cortex-a8
	TOOLCHAIN arm-linux-gnueabihf
	TESTING
	DRIVERS
		adc
		#barometer # all available barometer drivers
		barometer/bmp280
		batt_smbus
		camera_capture
		camera_trigger
		differential_pressure # all available differential pressure drivers
		distance_sensor # all available distance sensor drivers
		gps
		#imu # all available imu drivers
		imu/mpu9250
		linux_pwm_out
		#magnetometer # all available magnetometer drivers
		magnetometer/hmc5883
		pwm_out_sim
		rc_input
		#telemetry # all available telemetry drivers
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
		#load_mon
		local_position_estimator
		logger
		mavlink
		mc_att_control
		mc_pos_control
		mc_rate_control
		#micrortps_bridge
		navigator
		rc_update
		rover_pos_control
		sensors
		sih
		#simulator
		temperature_compensation
		vmount
		vtol_att_control
	SYSTEMCMDS
		dyn
		esc_calib
		led_control
		mixer
		motor_ramp
		param
		perf
		pwm
		reboot
		sd_bench
		shutdown
		tests # tests and test runner
		#top
		topic_listener
		tune_control
		ver
		work_queue
	EXAMPLES
		bottle_drop # OBC challenge
		dyn_hello # dynamically loading modules example
		fixedwing_control # Tutorial code from https://px4.io/dev/example_fixedwing_control
		hello
		#hwtest # Hardware test
		#matlab_csv_serial
		px4_mavlink_debug # Tutorial code from http://dev.px4.io/en/debug/debug_values.html
		px4_simple_app # Tutorial code from http://dev.px4.io/en/apps/hello_sky.html
		rover_steering_control # Rover example app
		uuv_example_app
	)
