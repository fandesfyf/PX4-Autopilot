#!/usr/bin/env python
import xml.etree.ElementTree as ET
import os

if len(os.sys.argv) != 2:
	print "Error in %s" % os.sys.argv[0]
	print "Usage: %s <parameters.xml>"
	raise SystemExit

fp_header = open("px4_parameters.h", "w")
fp_src = open("px4_parameters.c", "w")

tree = ET.parse(os.sys.argv[1])
root = tree.getroot()

# Generate the header file content
header = """
#include <stdint.h>
#include <systemlib/param/param.h>

// DO NOT EDIT
// This file is autogenerated from paramaters.xml

struct px4_parameters_t {
"""
start_name = ""
end_name = ""

for group in root:
	if group.tag == "group":
		header += """
	/*****************************************************************
	 * %s
	 ****************************************************************/""" % group.attrib["name"]
		for param in group:
			if not start_name:
				start_name = param.attrib["name"]
			end_name = param.attrib["name"]
			header += """
	const struct param_info_s __param__%s;""" % param.attrib["name"]
header += """
	const unsigned int param_count;
};

extern const struct px4_parameters_t px4_parameters;
"""

# Generate the C file content
src = """
#include <px4_parameters.h>

// DO NOT EDIT
// This file is autogenerated from paramaters.xml

static const
#ifndef __PX4_DARWIN
__attribute__((used, section("__param")))
#endif
struct px4_parameters_t px4_parameters_impl = {
"""
i=0
for group in root:
	if group.tag == "group":
		src += """
	/*****************************************************************
	 * %s
	 ****************************************************************/""" % group.attrib["name"]
		for param in group:
			if not start_name:
				start_name = param.attrib["name"]
			end_name = param.attrib["name"]
			i+=1
			src += """
	{
		"%s",
		PARAM_TYPE_%s,
		.val.f = %s
	},
""" % (param.attrib["name"], param.attrib["type"], param.attrib["default"])
src += """
	%d
};

#ifdef __PX4_DARWIN
#define ___param__attributes
#else
#define ___param__attributes __attribute__((alias("px4_parameters_impl")))
#endif

extern const struct px4_parameters_t px4_parameters ___param__attributes;
""" % i

fp_header.write(header)
fp_src.write(src)

