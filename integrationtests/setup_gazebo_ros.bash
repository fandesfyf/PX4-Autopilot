#!/bin/bash
#
# Setup environment to make PX4 visible to ROS/Gazebo.
#
# License: according to LICENSE.md in the root directory of the PX4 Firmware repository

if [ "$#" != 2 ]
then
    echo usage: source setup_gazebo_ros.bash src_dir build_dir
    echo ""
    return 1
fi

SRC_DIR=$1
BUILD_DIR=$2

# setup Gazebo env and update package path
export GAZEBO_RESOURCE_PATH=${SRC_DIR}/Tools/sitl_gazebo:${GAZEBO_RESOURCE_PATH}
export GAZEBO_MODEL_PATH=${SRC_DIR}/Tools/sitl_gazebo/models:${GAZEBO_MODEL_PATH}
export GAZEBO_PLUGIN_PATH=${BUILD_DIR}/build_gazebo:${GAZEBO_PLUGIN_PATH}
export LD_LIBRARY_PATH=${SRC_DIR}/Tools/sitl_gazebo/Build/msgs/:${BUILD_DIR}/build_gazebo:${LD_LIBRARY_PATH}:
export GAZEBO_MODEL_DATABASE_URI=""

echo -e "GAZEBO_RESOURCE_PATH $GAZEBO_RESOURCE_PATH"
echo -e "GAZEBO_MODEL_PATH $GAZEBO_MODEL_PATH"
echo -e "GAZEBO_PLUGIN_PATH $GAZEBO_PLUGIN_PATH"
echo -e "LD_LIBRARY_PATH $LD_LIBRARY_PATH"
