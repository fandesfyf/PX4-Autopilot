#
#   Copyright (C) 2012 PX4 Development Team. All rights reserved.
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

#
# Generic Makefile for PX4 firmware images.
#
# Requires:
#
# BOARD
#	Must be set to a board name known to the PX4 distribution (as
#	we need a corresponding NuttX export archive to link with).
#
# Optional:
#
# MODULES
#	Contains a list of module paths or path fragments used
#	to find modules. The names listed here are searched in
#	the following directories:
#		<absolute path>
#		$(MODULE_SEARCH_DIRS)
#		WORK_DIR
#		MODULE_SRC
#		PX4_MODULE_SRC
#
#	Application directories are expected to contain a module.mk
#	file which provides build configuration for the module. See
#	makefiles/module.mk for more details.
#
# BUILTIN_COMMANDS
#	Contains a list of built-in commands not explicitly provided
#	by modules / libraries. Each entry in this list is formatted
#	as <command>.<priority>.<stacksize>.<entrypoint>
#
# PX4_BASE:
#	Points to a PX4 distribution. Normally determined based on the
#	path to this file.
#
# CONFIG:
#	Used to set the output filename; defaults to 'firmware'.
#
# WORK_DIR:
#	Sets the directory in which the firmware will be built. Defaults
#	to the directory 'build' under the directory containing the
#	parent Makefile.
#
# ROMFS_ROOT:
#	If set to the path to a directory, a ROMFS image will be generated
#	containing the files under the directory and linked into the final
#	image.
#
# MODULE_SEARCH_DIRS:
#	Extra directories to search first for MODULES before looking in the
#	usual places.
#

################################################################################
# Paths and configuration
################################################################################

#
# Work out where this file is, so we can find other makefiles in the
# same directory.
#
# If PX4_BASE wasn't set previously, work out what it should be
# and set it here now.
#
MK_DIR	?= $(dir $(lastword $(MAKEFILE_LIST)))
ifeq ($(PX4_BASE),)
export PX4_BASE		:= $(abspath $(MK_DIR)/..)
endif
$(info %  PX4_BASE            = $(PX4_BASE))

#
# Set a default target so that included makefiles or errors here don't 
# cause confusion.
#
# XXX We could do something cute here with $(DEFAULT_GOAL) if it's not one
#     of the maintenance targets and set CONFIG based on it.
#
all:		firmware

#
# Get path and tool config
#
include $(MK_DIR)/setup.mk

#
# Locate the configuration file
#
ifeq ($(CONFIG),)
$(error Missing configuration name or file (specify with CONFIG=<config>))
endif
CONFIG_FILE		:= $(firstword $(wildcard $(CONFIG)) $(wildcard $(PX4_MK_DIR)/config_$(CONFIG).mk))
ifeq ($(CONFIG_FILE),)
$(error Can't find a config file called $(CONFIG) or $(PX4_MK_DIR)/config_$(CONFIG).mk)
endif
export CONFIG
include $(CONFIG_FILE)
$(info %  CONFIG              = $(CONFIG))

#
# Sanity-check the BOARD variable and then get the board config.
# If BOARD was not set by the configuration, extract it automatically.
#
# The board config in turn will fetch the toolchain configuration.
#
ifeq ($(BOARD),)
BOARD			:= $(firstword $(subst _, ,$(CONFIG)))
endif
BOARD_FILE		:= $(wildcard $(PX4_MK_DIR)/board_$(BOARD).mk)
ifeq ($(BOARD_FILE),)
$(error Config $(CONFIG) references board $(BOARD), but no board definition file found)
endif
export BOARD
include $(BOARD_FILE)
$(info %  BOARD               = $(BOARD))

#
# If WORK_DIR is not set, create a 'build' directory next to the
# parent Makefile.
#
PARENT_MAKEFILE		:= $(lastword $(filter-out $(lastword $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
ifeq ($(WORK_DIR),)
export WORK_DIR		:= $(dir $(PARENT_MAKEFILE))build/
endif
$(info %  WORK_DIR            = $(WORK_DIR))

#
# Things that, if they change, might affect everything
#
GLOBAL_DEPS		+= $(MAKEFILE_LIST)

#
# Extra things we should clean
#
EXTRA_CLEANS		 = 

################################################################################
# Modules
################################################################################

#
# We don't actually know what a module is called; all we have is a path fragment
# that we can search for, and where we expect to find a module.mk file.
#
# As such, we replicate the successfully-found path inside WORK_DIR for the
# module's build products in order to keep modules separated from each other.
#
# XXX If this becomes unwieldy or breaks for other reasons, we will need to 
#     move to allocating directory names and keeping tabs on makefiles via
#     the directory name. That will involve arithmetic (it'd probably be time
#     for GMSL).

# where to look for modules
MODULE_SEARCH_DIRS	 += $(WORK_DIR) $(MODULE_SRC) $(PX4_MODULE_SRC)

# sort and unique the modules list
MODULES			:= $(sort $(MODULES))

# locate the first instance of a module by full path or by looking on the
# module search path
define MODULE_SEARCH
	$(abspath $(firstword $(wildcard $(1)/module.mk) \
		$(foreach search_dir,$(MODULE_SEARCH_DIRS),$(wildcard $(search_dir)/$(1)/module.mk)) \
		MISSING_$1))
endef

# make a list of module makefiles and check that we found them all
MODULE_MKFILES		:= $(foreach module,$(MODULES),$(call MODULE_SEARCH,$(module)))
MISSING_MODULES		:= $(subst MISSING_,,$(filter MISSING_%,$(MODULE_MKFILES)))
ifneq ($(MISSING_MODULES),)
$(error Can't find module(s): $(MISSING_MODULES))
endif

# Make a list of the object files we expect to build from modules
# Note that this path will typically contain a double-slash at the WORK_DIR boundary; this must be
# preserved as it is used below to get the absolute path for the module.mk file correct.
#
MODULE_OBJS		:= $(foreach path,$(dir $(MODULE_MKFILES)),$(WORK_DIR)$(path)module.pre.o)

# rules to build module objects
.PHONY: $(MODULE_OBJS)
$(MODULE_OBJS):		relpath = $(patsubst $(WORK_DIR)%,%,$@)
$(MODULE_OBJS):		mkfile = $(patsubst %module.pre.o,%module.mk,$(relpath))
$(MODULE_OBJS):		$(GLOBAL_DEPS) $(NUTTX_CONFIG_HEADER)
	@$(ECHO) %%
	@$(ECHO) %% Building module using $(mkfile)
	@$(ECHO) %%
	$(Q) $(MAKE) -f $(PX4_MK_DIR)module.mk \
		MODULE_WORK_DIR=$(dir $@) \
		MODULE_OBJ=$@ \
		MODULE_MK=$(mkfile) \
		module

# make a list of phony clean targets for modules
MODULE_CLEANS		:= $(foreach path,$(dir $(MODULE_MKFILES)),$(WORK_DIR)$(path)/clean)

# rules to clean modules
.PHONY: $(MODULE_CLEANS)
$(MODULE_CLEANS):	relpath = $(patsubst $(WORK_DIR)%,%,$@)
$(MODULE_CLEANS):	mkfile = $(patsubst %clean,%module.mk,$(relpath))
$(MODULE_CLEANS):
	@$(ECHO) %% cleaning using $(mkfile)
	$(Q) $(MAKE) -f $(PX4_MK_DIR)module.mk \
	MODULE_WORK_DIR=$(dir $@) \
	MODULE_MK=$(mkfile) \
	clean

################################################################################
# NuttX libraries and paths
################################################################################

include $(PX4_MK_DIR)/nuttx.mk

################################################################################
# ROMFS generation
################################################################################

ifneq ($(ROMFS_ROOT),)

#
# Note that there is no support for more than one root directory or constructing
# a root from several templates. That would be a nice feature.
#

# Add dependencies on anything in the ROMFS root
ROMFS_DEPS		+= $(wildcard \
			     (ROMFS_ROOT)/* \
			     (ROMFS_ROOT)/*/* \
			     (ROMFS_ROOT)/*/*/* \
			     (ROMFS_ROOT)/*/*/*/* \
			     (ROMFS_ROOT)/*/*/*/*/* \
			     (ROMFS_ROOT)/*/*/*/*/*/*)
ROMFS_IMG		 = $(WORK_DIR)romfs.img
ROMFS_CSRC		 = $(ROMFS_IMG:.img=.c)
ROMFS_OBJ		 = $(ROMFS_CSRC:.c=.o)
LIBS			+= $(ROMFS_OBJ)
LINK_DEPS		+= $(ROMFS_OBJ)

# Turn the ROMFS image into an object file
$(ROMFS_OBJ): $(ROMFS_IMG) $(GLOBAL_DEPS)
	$(call BIN_TO_OBJ,$<,$@,romfs_img)

# Generate the ROMFS image from the root
$(ROMFS_IMG): $(ROMFS_DEPS) $(GLOBAL_DEPS)
	@$(ECHO) %% generating $@
	$(Q) $(GENROMFS) -f $@ -d $(ROMFS_ROOT) -V "NSHInitVol"

EXTRA_CLEANS		+= $(ROMGS_OBJ) $(ROMFS_IMG)

endif

################################################################################
# Builtin command list generation
################################################################################

#
# Builtin commands can be generated by the configuration, in which case they
# must refer to commands that already exist, or indirectly generated by modules
# when they are built.
#
# The configuration supplies builtin command information in the BUILTIN_COMMANDS
# variable. Applications make empty files in $(WORK_DIR)/builtin_commands whose
# filename contains the same information.
#
# In each case, the command information consists of four fields separated with a
# period. These fields are the command's name, its thread priority, its stack size
# and the name of the function to call when starting the thread.
#
BUILTIN_CSRC		 = $(WORK_DIR)builtin_commands.c

# add command definitions from modules
BUILTIN_COMMAND_FILES	:= $(wildcard $(WORK_DIR)builtin_commands/COMMAND.*)
BUILTIN_COMMANDS	+= $(subst COMMAND.,,$(notdir $(BUILTIN_COMMAND_FILES)))

ifneq ($(BUILTIN_COMMANDS),)

# (BUILTIN_PROTO,<cmdspec>,<outputfile>)
define BUILTIN_PROTO
	$(ECHO) 'extern int $(word 4,$1)(int argc, char *argv[]);' >> $2;
endef

# (BUILTIN_DEF,<cmdspec>,<outputfile>)
define BUILTIN_DEF
	$(ECHO) '    {"$(word 1,$1)", $(word 2,$1), $(word 3,$1), $(word 4,$1)},' >> $2;
endef

# Don't generate until modules have updated their command files
$(BUILTIN_CSRC):	$(GLOBAL_DEPS) $(BUILTIN_COMMAND_FILES)
	@$(ECHO) %% generating $@
	$(Q) $(ECHO) '/* builtin command list - automatically generated, do not edit */' > $@
	$(Q) $(ECHO) '#include <nuttx/config.h>' >> $@
	$(Q) $(ECHO) '#include <nuttx/binfmt/builtin.h>' >> $@
	$(Q) $(foreach spec,$(BUILTIN_COMMANDS),$(call BUILTIN_PROTO,$(subst ., ,$(spec)),$@))
	$(Q) $(ECHO) 'const struct builtin_s g_builtins[] = {' >> $@
	$(Q) $(foreach spec,$(BUILTIN_COMMANDS),$(call BUILTIN_DEF,$(subst ., ,$(spec)),$@))
	$(Q) $(ECHO) '    {NULL, 0, 0, NULL}' >> $@
	$(Q) $(ECHO) '};' >> $@
	$(Q) $(ECHO) 'const int g_builtin_count = $(words $(BUILTIN_COMMANDS));' >> $@

SRCS			+= $(BUILTIN_CSRC)

EXTRA_CLEANS		+= $(BUILTIN_CSRC)

endif

################################################################################
# Default SRCS generation
################################################################################

#
# If there are no SRCS, the build will fail; in that case, generate an empty
# source file.
#
ifeq ($(SRCS),)
EMPTY_SRC		 = $(WORK_DIR)empty.c
$(EMPTY_SRC):
	$(Q) $(ECHO) '/* this is an empty file */' > $@

SRCS			+= $(EMPTY_SRC)
endif

################################################################################
# Build rules
################################################################################

#
# What we're going to build.
#
PRODUCT_BUNDLE		 = $(WORK_DIR)firmware.px4
PRODUCT_BIN		 = $(WORK_DIR)firmware.bin
PRODUCT_SYM		 = $(WORK_DIR)firmware.sym

.PHONY:			firmware
firmware:		$(PRODUCT_BUNDLE)

#
# Object files we will generate from sources
#
OBJS			:= $(foreach src,$(SRCS),$(WORK_DIR)$(src).o)

#
# SRCS -> OBJS rules
#

$(OBJS):		$(GLOBAL_DEPS)

$(filter %.c.o,$(OBJS)): $(WORK_DIR)%.c.o: %.c $(GLOBAL_DEPS)
	$(call COMPILE,$<,$@)

$(filter %.cpp.o,$(OBJS)): $(WORK_DIR)%.cpp.o: %.cpp $(GLOBAL_DEPS)
	$(call COMPILEXX,$<,$@)

$(filter %.S.o,$(OBJS)): $(WORK_DIR)%.S.o: %.S $(GLOBAL_DEPS)
	$(call ASSEMBLE,$<,$@)

#
# Built product rules
#

$(PRODUCT_BUNDLE):	$(PRODUCT_BIN)
	@$(ECHO) %% Generating $@
	$(Q) $(MKFW) --prototype $(IMAGE_DIR)/$(BOARD).prototype \
		--git_identity $(PX4_BASE) \
		--image $< > $@

$(PRODUCT_BIN):		$(PRODUCT_SYM)
	$(call SYM_TO_BIN,$<,$@)

$(PRODUCT_SYM):		$(OBJS) $(MODULE_OBJS) $(GLOBAL_DEPS) $(LINK_DEPS) $(MODULE_MKFILES)
	$(call LINK,$@,$(OBJS) $(MODULE_OBJS))

#
# Utility rules
#

.PHONY: upload
upload:	$(PRODUCT_BUNDLE) $(PRODUCT_BIN)
	$(Q) $(MAKE) -f $(PX4_MK_DIR)/upload.mk \
		METHOD=serial \
		PRODUCT=$(PRODUCT) \
		BUNDLE=$(PRODUCT_BUNDLE) \
		BIN=$(PRODUCT_BIN)

.PHONY: clean
clean:			$(MODULE_CLEANS)
	@$(ECHO) %% cleaning
	$(Q) $(REMOVE) $(PRODUCT_BUNDLE) $(PRODUCT_BIN) $(PRODUCT_SYM)
	$(Q) $(REMOVE) $(OBJS) $(DEP_INCLUDES) $(EXTRA_CLEANS)
	$(Q) $(RMDIR) $(NUTTX_EXPORT_DIR)

#
# DEP_INCLUDES is defined by the toolchain include in terms of $(OBJS)
#
-include $(DEP_INCLUDES)
