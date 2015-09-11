include(posix/px4_impl_posix-arm)

function(px4_get_config)

	px4_parse_function_args(
		NAME px4_set_config_modules
		ONE_VALUE OUT_MODULES OUT_FW_OPTS OUT_EXTRA_CMDS
		ARGN ${ARGN})

	set(config_module_list
		drivers/device
		drivers/boards/sitl
		drivers/led

		systemcmds/param
		systemcmds/ver

		modules/mavlink

		modules/systemlib
		modules/uORB
		modules/sensors
		modules/dataman
		modules/sdlog2
		modules/simulator
		modules/commander

		lib/mathlib
		lib/mathlib/math/filter
		lib/geo
		lib/geo_lookup
		lib/conversion

		platforms/posix/px4_layer
		platforms/posix/work_queue
		modules/muorb/krait
		)

	set(${out_module_list} ${config_module_list} PARENT_SCOPE)

endfunction()

