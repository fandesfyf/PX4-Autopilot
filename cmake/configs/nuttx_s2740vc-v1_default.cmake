include(nuttx/px4_impl_nuttx)

add_definitions(
	-DPARAM_NO_ORB
	-DPARAM_NO_AUTOSAVE
	)

px4_nuttx_configure(HWCLASS m4 CONFIG nsh)

#
# UAVCAN boot loadable Module ID

set(uavcanblid_sw_version_major 0)
set(uavcanblid_sw_version_minor 1)
add_definitions(
	-DAPP_VERSION_MAJOR=${uavcanblid_sw_version_major}
	-DAPP_VERSION_MINOR=${uavcanblid_sw_version_minor}
	)

#
# Bring in common uavcan hardware identity definitions
#
include(common/px4_git)
px4_add_git_submodule(TARGET git_uavcan_board_ident PATH "cmake/configs/uavcan_board_ident")
include(configs/uavcan_board_ident/s2740vc-v1)

# N.B. this would be uncommented when there is an APP
#px4_nuttx_make_uavcan_bootloadable(BOARD ${BOARD}
# BIN ${CMAKE_CURRENT_BINARY_DIR}/src/firmware/nuttx/s2740vc-v1.bin
# HWNAME ${uavcanblid_name}
# HW_MAJOR ${uavcanblid_hw_version_major}
# HW_MINOR ${uavcanblid_hw_version_minor}
# SW_MAJOR ${uavcanblid_sw_version_major}
# SW_MINOR ${uavcanblid_sw_version_minor})

set(config_module_list

	#
	# Board support modules
	#

	drivers/stm32
	drivers/led
	drivers/boards

	#
	# System commands
	#
	systemcmds/reboot
	systemcmds/top
	systemcmds/config
	systemcmds/ver

	#
	# General system control
	#

	#
	# Library modules
	#
	modules/systemlib/param
	modules/systemlib
	lib/version

	#
	# Libraries
	#
	# had to add for cmake, not sure why wasn't in original config
	platforms/nuttx
	platforms/common
	platforms/nuttx/px4_layer
)
