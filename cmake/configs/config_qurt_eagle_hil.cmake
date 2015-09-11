include(qurt/px4_impl_qurt)

function(px4_get_config out_module_list)

	set(config_module_list
		drivers/device
		drivers/boards/sitl
		drivers/pwm_out_sim
		drivers/led
		drivers/rgbled
		modules/sensors

		#
		# System commands
		#
		systemcmds/param
		systemcmds/mixer

		#
		# Estimation modules (EKF/ SO3 / other filters)
		#
		#modules/attitude_estimator_ekf
		modules/ekf_att_pos_estimator
		modules/attitude_estimator_q
		modules/position_estimator_inav

		#
		# Vehicle Control
		#
		modules/mc_att_control
		modules/mc_pos_control

		#
		# Library modules
		#
		modules/systemlib
		modules/systemlib/mixer
		modules/uORB
		modules/commander

		#
		# Libraries
		#
		lib/mathlib
		lib/mathlib/math/filter
		lib/geo
		lib/geo_lookup
		lib/conversion
		modules/controllib

		#
		# QuRT port
		#
		platforms/common
		platforms/qurt/px4_layer
		platforms/posix/work_queue

		#
		# sources for muorb over fastrpc
		#
		modules/muorb/adsp
		)
	set(${out_module_list} ${config_module_list} PARENT_SCOPE)

endfunction()

