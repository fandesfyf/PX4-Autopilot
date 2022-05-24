px4_add_module(
	MODULE modules__under_water_control
	MAIN under_water_control
	STACK_MAIN 2400
	COMPILE_FLAGS
	SRCS
		uartsensor.cpp
		under_water_control.cpp
		servor_control.cpp
	DEPENDS
	)
