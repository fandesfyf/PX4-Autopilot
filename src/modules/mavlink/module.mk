############################################################################
#
#   Copyright (c) 2012-2014 PX4 Development Team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name PX4 nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

#
# MAVLink protocol to uORB interface process
#

MODULE_COMMAND  = mavlink

SRCS 		 +=	mavlink.c \
		  	mavlink_main.cpp \
			mavlink_mission.cpp \
			mavlink_parameters.cpp \
			mavlink_orb_subscription.cpp \
			mavlink_messages.cpp \
			mavlink_stream.cpp \
			mavlink_rate_limiter.cpp \
			mavlink_receiver.cpp \
			mavlink_ftp.cpp 

INCLUDE_DIRS	 += $(MAVLINK_SRC)/include/mavlink

MAXOPTIMIZATION	 = -Os

MODULE_STACKSIZE = 1200

EXTRACXXFLAGS	= -Weffc++ -Wno-attributes -Wno-packed -DMAVLINK_COMM_NUM_BUFFERS=3

EXTRACFLAGS	= -Wno-packed -DMAVLINK_COMM_NUM_BUFFERS=3
