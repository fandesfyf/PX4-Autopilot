px4_add_module(
	MODULE modules__under_water_control
	MAIN under_water_control
	STACK_MAIN 2400
	COMPILE_FLAGS
	SRCS
		under_water_control.cpp
	DEPENDS
	)
