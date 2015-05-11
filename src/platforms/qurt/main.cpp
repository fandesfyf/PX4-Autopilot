/****************************************************************************
 *
 *   Copyright (C) 2015 Mark Charlebois. All rights reserved.
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
 * @file main.cpp
 * Basic shell to execute builtin "apps" 
 *
 * @author Mark Charlebois <charlebm@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <hexagon_standalone.h>

//using namespace std;

//typedef int (*px4_main_t)(int argc, char *argv[]);

#include "apps.h"
#include "px4_middleware.h"

static const char *commands = 
"x\n"
"hello start"
#if 0
"uorb start\n"
"simulator start -s\n"
"barosim start\n"
"adcsim start\n"
"accelsim start\n"
"gyrosim start\n"
"param set CAL_GYRO0_ID 2293760\n"
"param set CAL_ACC0_ID 1310720\n"
"param set CAL_ACC1_ID 1376256\n"
"param set CAL_MAG0_ID 196608\n"
"rgbled start\n"
"mavlink start -d /tmp/ttyS0\n"
"sensors start\n"
"hil mode_pwm\n"
"commander start\n"
"list_devices\n"
"list_topics\n"
#endif
;


static void run_cmd(const vector<string> &appargs) {
	// command is appargs[0]
	string command = appargs[0];
	printf("Looking for %s\n", command.c_str());
	if (apps.find(command) != apps.end()) {
		const char *arg[2+1];

		unsigned int i = 0;
		printf("size = %d\n", appargs.size());
		while (i < appargs.size() && appargs[i].c_str()[0] != '\0') {
			arg[i] = (char *)appargs[i].c_str();
			printf("  arg = '%s'\n", arg[i]);
			++i;
		}
		arg[i] = (char *)0;
		//printf("BEFORE argc = %d %s %s %p\n", i, arg[0], arg[1], arg[2]);
		apps[command](i,(char **)arg);
		//printf("AFTER argc = %d %s %s %p\n", i, arg[0], arg[1], arg[2]);
	}
	else
	{
		cout << "Invalid command" << endl;
		list_builtins();
	}
}

void eat_whitespace(const char *&b, int &i)
{
	// Eat whitespace
	while (b[i] == ' ' || b[i] == '\t') { ++i; }
	b = &b[i];
	i=0;
}

static void process_commands(const char *cmds)
{
	vector<string> appargs;
	int i=0;
	const char *b = cmds;
	bool found_first_char = false;
	char arg[20];

	// Eat leading whitespace
	eat_whitespace(b, i);

	for(;;) {
		// End of command line
		if (b[i] == '\n' || b[i] == '\0') {
			strncpy(arg, b, i);
			arg[i] = '\0';
			appargs.push_back(arg);

			// If we have a command to run
			if (appargs.size() > 0) {
				run_cmd(appargs);
			}
			appargs.clear();
			if (b[i] == '\n') {
				eat_whitespace(b, ++i);
				continue;
			}
			else {
				break;
			}
		}
		// End of arg
		else if (b[i] == ' ') {
			strncpy(arg, b, i);
			arg[i] = '\0';
			appargs.push_back(arg);
			eat_whitespace(b, ++i);
			continue;
		}
		++i;
	}
}

namespace px4 {
extern void init_once(void);
};

int main(int argc, char **argv)
{
	px4::init_once();
	px4::init(argc, argv, "mainapp");
	process_commands(commands);
	for (;;) {}
}
