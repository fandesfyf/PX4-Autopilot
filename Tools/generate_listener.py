#!/usr/bin/python

import glob
import os
import sys

# This script is run from Build/<target>_default.build/$(PX4_BASE)/Firmware/src/systemcmds/topic_listener

# argv[1] must be the full path of the top Firmware dir

raw_messages = glob.glob(sys.argv[1]+"/msg/*.msg")
messages = []
message_elements = []


for index,m in enumerate(raw_messages):
	temp_list_floats = []
	temp_list_uint64 = []
	temp_list_bool = []
	if("pwm_input" not in m and "position_setpoint" not in m):
		temp_list = []
		f = open(m,'r')
		for line in f.readlines():
			if ('float32[' in line.split(' ')[0]):
				num_floats = int(line.split(" ")[0].split("[")[1].split("]")[0])
				temp_list.append(("float_array",line.split(' ')[1].split('\t')[0].split('\n')[0],num_floats))
			elif ('float64[' in line.split(' ')[0]):
				num_floats = int(line.split(" ")[0].split("[")[1].split("]")[0])
				temp_list.append(("double_array",line.split(' ')[1].split('\t')[0].split('\n')[0],num_floats))
			elif ('uint64[' in line.split(' ')[0]):
				num_floats = int(line.split(" ")[0].split("[")[1].split("]")[0])
				temp_list.append(("uint64_array",line.split(' ')[1].split('\t')[0].split('\n')[0],num_floats))
			elif(line.split(' ')[0] == "float32"):
				temp_list.append(("float",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "float64"):
				temp_list.append(("double",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "uint64") and len(line.split('=')) == 1:
				temp_list.append(("uint64",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "uint32") and len(line.split('=')) == 1:
				temp_list.append(("uint32",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "uint16") and len(line.split('=')) == 1:
				temp_list.append(("uint16",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "int64") and len(line.split('=')) == 1:
				temp_list.append(("int64",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "int32") and len(line.split('=')) == 1:
				temp_list.append(("int32",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif(line.split(' ')[0] == "int16") and len(line.split('=')) == 1:
				temp_list.append(("int16",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif (line.split(' ')[0] == "bool") and len(line.split('=')) == 1:
				temp_list.append(("bool",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif (line.split(' ')[0] == "uint8") and len(line.split('=')) == 1:
				temp_list.append(("uint8",line.split(' ')[1].split('\t')[0].split('\n')[0]))
			elif (line.split(' ')[0] == "int8") and len(line.split('=')) == 1:
				temp_list.append(("int8",line.split(' ')[1].split('\t')[0].split('\n')[0]))

		f.close()
		(m_head, m_tail) = os.path.split(m)
		message = m_tail.split('.')[0]
		if message != "actuator_controls":
			messages.append(message)
			message_elements.append(temp_list)
		#messages.append(m.split('/')[-1].split('.')[0])

num_messages = len(messages);

print("""

/****************************************************************************
 *
 *   Copyright (c) 2015 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file topic_listener.cpp
 *
 * Autogenerated by Tools/generate_listener.py
 *
 * Tool for listening to topics when running flight stack on linux.
 */

#include <px4_middleware.h>
#include <px4_app.h>
#include <px4_config.h>
#include <uORB/uORB.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

""")
for m in messages:
	print("#include <uORB/topics/%s.h>" % m)


print("""
extern "C" __EXPORT int listener_main(int argc, char *argv[]);

int listener_main(int argc, char *argv[]) {
	int sub = -1;
	orb_id_t ID;
	if(argc < 2) {
		printf("need at least two arguments: topic name. [optional number of messages to print]\\n");
		return 1;
	}
""")
print("\tunsigned num_msgs = (argc > 2) ? atoi(argv[2]) : 1;")
print("\tif (strncmp(argv[1],\"%s\",50) == 0) {" % messages[0])
print("\t\tsub = orb_subscribe(ORB_ID(%s));" % messages[0])
print("\t\tID = ORB_ID(%s);" % messages[0])
print("\t\tstruct %s_s container;" % messages[0])
print("\t\tmemset(&container, 0, sizeof(container));")
for index,m in enumerate(messages[1:]):
	print("\t} else if (strncmp(argv[1],\"%s\",50) == 0) {" % m)
	print("\t\tsub = orb_subscribe(ORB_ID(%s));" % m)
	print("\t\tID = ORB_ID(%s);" % m)
	print("\t\tstruct %s_s container;" % m)
	print("\t\tmemset(&container, 0, sizeof(container));")
	print("\t\tbool updated;")
	print("\t\tint i = 0;")
	print("\t\twhile(i < num_msgs) {")
	print("\t\t\torb_check(sub,&updated);")
	print("\t\t\tif (i == 0) { updated = true; } else { usleep(500); }")
	print("\t\t\ti++;")
	print("\t\t\tif (updated) {")
	print("\t\tprintf(\"\\nTOPIC: %s #%%d\\n\", i);" % m)
	print("\t\t\torb_copy(ID,sub,&container);")
	for item in message_elements[index+1]:
		if item[0] == "float":
			print("\t\t\tprintf(\"%s: %%8.4f\\n\",(double)container.%s);" % (item[1], item[1]))
		elif item[0] == "float_array":
			print("\t\t\tprintf(\"%s: \");" % item[1])
			print("\t\t\tfor (int j = 0; j < %d; j++) {" % item[2])
			print("\t\t\t\tprintf(\"%%8.4f \",(double)container.%s[j]);" % item[1])
			print("\t\t\t}")
			print("\t\t\tprintf(\"\\n\");")
		elif item[0] == "double":
			print("\t\t\tprintf(\"%s: %%8.4f\\n\",(double)container.%s);" % (item[1], item[1]))
		elif item[0] == "double_array":
			print("\t\t\tprintf(\"%s: \");" % item[1])
			print("\t\t\tfor (int j = 0; j < %d; j++) {" % item[2])
			print("\t\t\t\tprintf(\"%%8.4f \",(double)container.%s[j]);" % item[1])
			print("\t\t\t}")
			print("\t\t\tprintf(\"\\n\");")
		elif item[0] == "uint64":
			print("\t\t\tprintf(\"%s: %%\" PRIu64 \"\\n\",container.%s);" % (item[1], item[1]))
		elif item[0] == "uint64_array":
			print("\t\t\tprintf(\"%s: \");" % item[1])
			print("\t\t\tfor (int j = 0; j < %d; j++) {" % item[2])
			print("\t\t\t\tprintf(\"%%\" PRIu64 \"\",container.%s[j]);" % item[1])
			print("\t\t\t}")
			print("\t\t\tprintf(\"\\n\");")
		elif item[0] == "int64":
			print("\t\t\tprintf(\"%s: %%\" PRId64 \"\\n\",container.%s);" % (item[1], item[1]))
		elif item[0] == "int32":
			print("\t\t\tprintf(\"%s: %%d\\n\",container.%s);" % (item[1], item[1]))
		elif item[0] == "uint32":
			print("\t\t\tprintf(\"%s: %%d\\n\",container.%s);" % (item[1], item[1]))
		elif item[0] == "uint8":
			print("\t\t\tprintf(\"%s: %%u\\n\",container.%s);" % (item[1], item[1]))
		elif item[0] == "bool":
			print("\t\t\tprintf(\"%s: %%s\\n\",container.%s ? \"True\" : \"False\");" % (item[1], item[1]))
	print("\t\t\t}")
	print("\t\t}")
print("\t} else {")
print("\t\t printf(\" Topic did not match any known topics\\n\");")
print("\t}")
print("\t\torb_unsubscribe(sub);")
print("\t return 0;")

print("}")
