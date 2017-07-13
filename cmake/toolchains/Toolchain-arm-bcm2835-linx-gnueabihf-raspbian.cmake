# defines:
#
# NM
# OBJCOPY
# LD
# CXX_COMPILER
# C_COMPILER
# CMAKE_SYSTEM_NAME
# CMAKE_SYSTEM_VERSION
# LINKER_FLAGS
# CMAKE_EXE_LINKER_FLAGS
# CMAKE_FIND_ROOT_PATH
# CMAKE_FIND_ROOT_PATH_MODE_PROGRAM
# CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
# CMAKE_FIND_ROOT_PATH_MODE_INCLUDE

include(CMakeForceCompiler)

if ("$ENV{RPI_TOOLCHAIN_DIR}" STREQUAL "")
        message(FATAL_ERROR "RPI_TOOLCHAIN_DIR not set")
else()
        set(RPI_TOOLCHAIN_DIR $ENV{RPI_TOOLCHAIN_DIR})
endif()

if ("$ENV{CROSS_COMPILE_PREFIX}" STREQUAL "")
        message(FATAL_ERROR "CROSS_COMPILE_PREFIX not set")
else()
        set(CROSS_COMPILE_PREFIX $ENV{CROSS_COMPILE_PREFIX})
endif()





# this one is important
set(CMAKE_SYSTEM_NAME Generic)

#this one not so much
set(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
find_program(C_COMPILER ${CROSS_COMPILE_PREFIX}-gcc
	PATHS ${RPI_TOOLCHAIN_DIR}/bin
	NO_DEFAULT_PATH
	)

if(NOT C_COMPILER)
	message(FATAL_ERROR "could not find ${CROSS_COMPILE_PREFIX}-gcc compiler")
endif()
cmake_force_c_compiler(${C_COMPILER} GNU)

find_program(CXX_COMPILER  ${CROSS_COMPILE_PREFIX}-g++
	PATHS ${RPI_TOOLCHAIN_DIR}/bin
	NO_DEFAULT_PATH
	)

if(NOT CXX_COMPILER)
	message(FATAL_ERROR "could not find ${CROSS_COMPILE_PREFIX}-g++ compiler")
endif()
cmake_force_cxx_compiler(${CXX_COMPILER} GNU)

# compiler tools
foreach(tool objcopy nm ld)
	string(TOUPPER ${tool} TOOL)
	find_program(${TOOL} ${CROSS_COMPILE_PREFIX}-${tool}
		PATHS ${RPI_TOOLCHAIN_DIR}/bin
		NO_DEFAULT_PATH
		)
	if(NOT ${TOOL})
		message(FATAL_ERROR "could not find ${CROSS_COMPILE_PREFIX}-${tool}")
	endif()
endforeach()

# os tools
foreach(tool echo grep rm mkdir nm cp touch make unzip)
	string(TOUPPER ${tool} TOOL)
	find_program(${TOOL} ${tool})
	if(NOT ${TOOL})
		message(FATAL_ERROR "could not find ${TOOL}")
	endif()
endforeach()

add_definitions(
	-D __DF_RPI
	)

set(LINKER_FLAGS "-Wl,-gc-sections")
set(CMAKE_EXE_LINKER_FLAGS ${LINKER_FLAGS})
set(CMAKE_C_FLAGS ${C_FLAGS})
set(CMAKE_CXX_LINKER_FLAGS ${C_FLAGS})

# where is the target environment
set(CMAKE_FIND_ROOT_PATH  get_file_component(${C_COMPILER} PATH))

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
