include(qurt/px4_impl_qurt)

if ("$ENV{HEXAGON_SDK_ROOT}" STREQUAL "")
	message(FATAL_ERROR "Enviroment variable HEXAGON_SDK_ROOT must be set")
else()
	set(HEXAGON_SDK_ROOT $ENV{HEXAGON_SDK_ROOT})
endif()

set(DISABLE_PARAMS_MODULE_SCOPING TRUE)

# Get $QC_SOC_TARGET from environment if existing.
if (DEFINED ENV{QC_SOC_TARGET})
	set(QC_SOC_TARGET $ENV{QC_SOC_TARGET})
else()
	set(QC_SOC_TARGET "APQ8074")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PX4_SOURCE_DIR}/cmake/cmake_hexagon")
include(toolchain/Toolchain-qurt)
include(qurt_flags)
include_directories(${HEXAGON_SDK_INCLUDES})

set(config_module_list
	drivers/device
	drivers/boards
	drivers/pwm_out_sim
	drivers/led
	drivers/rgbled
	modules/sensors

	#
	# System commands
	#
	systemcmds/param
	systemcmds/led
	systemcmds/mixer

	#
	# Estimation modules
	#
	modules/attitude_estimator_q
	modules/position_estimator_inav
	modules/local_position_estimator
	modules/ekf2

	#
	# Vehicle Control
	#
	modules/mc_att_control
	modules/mc_pos_control

	#
	# Library modules
	#
	modules/systemlib/param
	modules/systemlib
	modules/uORB
	modules/commander

	#
	# Libraries
	#
	lib/controllib
	lib/conversion
	lib/DriverFramework/framework
	lib/geo
	lib/geo_lookup
	lib/led
	lib/mathlib
	lib/mathlib/math/filter
	lib/mixer
	lib/runway_takeoff
	lib/tailsitter_recovery
	lib/terrain_estimation
	lib/version

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
