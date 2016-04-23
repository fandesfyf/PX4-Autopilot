include(posix/px4_impl_posix)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchains/Toolchain-arm-linux-gnueabihf.cmake)

set(CONFIG_SHMEM "1")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/cmake_hexagon")

# A temporary build option to support the use of the legacy (non DriverFramework) drivers.
add_definitions(
   -D__USING_SNAPDRAGON_LEGACY_DRIVER
   )

set(config_module_list
	drivers/device
	drivers/boards/sitl
	drivers/led

	systemcmds/param
	systemcmds/ver

	modules/mavlink

	modules/param
	modules/systemlib
	modules/uORB
	modules/dataman
	modules/sdlog2
	modules/simulator
	modules/commander

	lib/controllib
	lib/mathlib
	lib/mathlib/math/filter
	lib/geo
	lib/geo_lookup
	lib/conversion

	platforms/common
	platforms/posix/px4_layer
	platforms/posix/work_queue

	modules/muorb/krait
	)

