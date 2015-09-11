include(qurt/px4_impl_qurt)

function(px4_get_config)

	px4_parse_function_args(
		NAME px4_set_config_modules
		ONE_VALUE OUT_MODULES
		REQUIRED OUT_MODULES
		ARGN ${ARGN})

	set(config_module_list
		drivers/device

		#
		# System commands
		#
		systemcmds/param

		#
		# Library modules
		#
		modules/systemlib
		modules/uORB

		#
		# QuRT port
		#
		platforms/common
		platforms/qurt/px4_layer
		platforms/posix/work_queue
		platforms/qurt/tests/hello
		)
	set(${out_module_list} ${config_module_list} PARENT_SCOPE)

	# output
	set(${OUT_MODULES} ${config_module_list} PARENT_SCOPE)

endfunction()

