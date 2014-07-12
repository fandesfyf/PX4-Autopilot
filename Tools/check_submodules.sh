#!/bin/sh

if [ -d NuttX/nuttx ];
	then
	STATUSRETVAL=$(git submodule summary | grep -A20 -i "NuttX" | grep "<")
	if [ -z "$STATUSRETVAL" ]; then
		echo "Checked NuttX submodule, correct version found"
	else
		echo ""
		echo ""
		echo "NuttX sub repo not at correct version. Try 'git submodule update'"
		echo "or follow instructions on http://pixhawk.org/dev/git/submodules"
		echo ""
		echo ""
		echo "New commits required:"
		echo "$(git submodule summary)"
		echo ""
		exit 1
	fi
else
	git submodule init;
	git submodule update;
fi


if [ -d mavlink/include/mavlink/v1.0 ];
	then
	STATUSRETVAL=$(git submodule summary | grep -A20 -i "mavlink/include/mavlink/v1.0" | grep "<")
	if [ -z "$STATUSRETVAL" ]; then
		echo "Checked mavlink submodule, correct version found"
	else
		echo ""
		echo ""
		echo "mavlink sub repo not at correct version. Try 'git submodule update'"
		echo "or follow instructions on http://pixhawk.org/dev/git/submodules"
		echo ""
		echo ""
		echo "New commits required:"
		echo "$(git submodule summary)"
		echo ""
		exit 1
	fi
else
	git submodule init;
	git submodule update;
fi


if [ -d uavcan/libuavcan_drivers ];
then
	STATUSRETVAL=$(git status --porcelain | grep -i uavcan)
	if [ "$STATUSRETVAL" == "" ]; then
		echo "Checked uavcan submodule, correct version found"
	else
		echo "uavcan sub repo not at correct version. Try 'make updatesubmodules'"
		echo "or follow instructions on http://pixhawk.org/dev/git/submodules"
		exit 1
	fi
else
	git submodule init
	git submodule update
fi

exit 0
