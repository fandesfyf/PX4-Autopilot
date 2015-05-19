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
# Makefile for PX4 Nuttx based firmware images.
#

################################################################################
# ROMFS generation
################################################################################

ifneq ($(ROMFS_ROOT),)
ifeq ($(wildcard $(ROMFS_ROOT)),)
$(error ROMFS_ROOT specifies a directory that does not exist)
endif

#
# Note that there is no support for more than one root directory or constructing
# a root from several templates. That would be a nice feature.
#

# Add dependencies on anything in the ROMFS root directory
ROMFS_FILES		+= $(wildcard \
			     $(ROMFS_ROOT)/* \
			     $(ROMFS_ROOT)/*/* \
			     $(ROMFS_ROOT)/*/*/* \
			     $(ROMFS_ROOT)/*/*/*/* \
			     $(ROMFS_ROOT)/*/*/*/*/* \
			     $(ROMFS_ROOT)/*/*/*/*/*/*)
ifeq ($(ROMFS_FILES),)
$(error ROMFS_ROOT $(ROMFS_ROOT) specifies a directory containing no files)
endif
ROMFS_DEPS		+= $(ROMFS_FILES)

# Extra files that may be copied into the ROMFS /extras directory
# ROMFS_EXTRA_FILES are required, ROMFS_OPTIONAL_FILES are optional
ROMFS_EXTRA_FILES	+= $(wildcard $(ROMFS_OPTIONAL_FILES))
ROMFS_DEPS		+= $(ROMFS_EXTRA_FILES)

ROMFS_IMG		 = romfs.img
ROMFS_SCRATCH		 = romfs_scratch
ROMFS_CSRC		 = $(ROMFS_IMG:.img=.c)
ROMFS_OBJ		 = $(ROMFS_CSRC:.c=.o)
LIBS			+= $(ROMFS_OBJ)
LINK_DEPS		+= $(ROMFS_OBJ)

# Remove all comments from startup and mixer files
ROMFS_PRUNER	 = $(PX4_BASE)/Tools/px_romfs_pruner.py

# Turn the ROMFS image into an object file
$(ROMFS_OBJ): $(ROMFS_IMG) $(GLOBAL_DEPS)
	$(call BIN_TO_OBJ,$<,$@,romfs_img)

# Generate the ROMFS image from the root
$(ROMFS_IMG): $(ROMFS_SCRATCH) $(ROMFS_DEPS) $(GLOBAL_DEPS)
	@$(ECHO) "ROMFS:   $@"
	$(Q) $(GENROMFS) -f $@ -d $(ROMFS_SCRATCH) -V "NSHInitVol"

# Construct the ROMFS scratch root from the canonical root
$(ROMFS_SCRATCH): $(ROMFS_DEPS) $(GLOBAL_DEPS)
	$(Q) $(MKDIR) -p $(ROMFS_SCRATCH)
	$(Q) $(COPYDIR) $(ROMFS_ROOT)/* $(ROMFS_SCRATCH)
# delete all files in ROMFS_SCRATCH which start with a . or end with a ~
	$(Q) $(RM) $(ROMFS_SCRATCH)/*/.[!.]* $(ROMFS_SCRATCH)/*/*~
ifneq ($(ROMFS_EXTRA_FILES),)
	$(Q) $(MKDIR) -p $(ROMFS_SCRATCH)/extras
	$(Q) $(COPY) $(ROMFS_EXTRA_FILES) $(ROMFS_SCRATCH)/extras
endif
	$(Q) $(PYTHON) -u $(ROMFS_PRUNER) --folder $(ROMFS_SCRATCH)

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

# command definitions from modules (may be empty at Makefile parsing time...)
MODULE_COMMANDS		 = $(subst COMMAND.,,$(notdir $(wildcard $(WORK_DIR)builtin_commands/COMMAND.*)))

# We must have at least one pre-defined builtin command in order to generate
# any of this.
#
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
$(BUILTIN_CSRC):	$(GLOBAL_DEPS) $(MODULE_OBJS) $(MODULE_MKFILES) $(BUILTIN_COMMAND_FILES)
	@$(ECHO) "CMDS:    $@"
	$(Q) $(ECHO) '/* builtin command list - automatically generated, do not edit */' > $@
	$(Q) $(ECHO) '#include <nuttx/config.h>' >> $@
	$(Q) $(ECHO) '#include <nuttx/binfmt/builtin.h>' >> $@
	$(Q) $(foreach spec,$(BUILTIN_COMMANDS),$(call BUILTIN_PROTO,$(subst ., ,$(spec)),$@))
	$(Q) $(foreach spec,$(MODULE_COMMANDS),$(call BUILTIN_PROTO,$(subst ., ,$(spec)),$@))
	$(Q) $(ECHO) 'const struct builtin_s g_builtins[] = {' >> $@
	$(Q) $(foreach spec,$(BUILTIN_COMMANDS),$(call BUILTIN_DEF,$(subst ., ,$(spec)),$@))
	$(Q) $(foreach spec,$(MODULE_COMMANDS),$(call BUILTIN_DEF,$(subst ., ,$(spec)),$@))
	$(Q) $(ECHO) '    {NULL, 0, 0, NULL}' >> $@
	$(Q) $(ECHO) '};' >> $@
	$(Q) $(ECHO) 'const int g_builtin_count = $(words $(BUILTIN_COMMANDS) $(MODULE_COMMANDS));' >> $@

SRCS			+= $(BUILTIN_CSRC)

EXTRA_CLEANS		+= $(BUILTIN_CSRC)

endif

################################################################################
# Build rules
################################################################################

#
# What we're going to build.
#
PRODUCT_BUNDLE		 = $(WORK_DIR)firmware.px4
PRODUCT_BIN		 = $(WORK_DIR)firmware.bin
PRODUCT_ELF		 = $(WORK_DIR)firmware.elf
PRODUCT_PARAMXML = $(WORK_DIR)/parameters.xml

.PHONY:			firmware
firmware:		$(PRODUCT_BUNDLE)

#
# Built product rules
#

$(PRODUCT_BUNDLE):	$(PRODUCT_BIN)
	@$(ECHO) %% Generating $@
ifdef GEN_PARAM_XML
	python $(PX4_BASE)/Tools/px_process_params.py --src-path $(PX4_BASE)/src --board CONFIG_ARCH_BOARD_$(CONFIG_BOARD) --xml
	$(Q) $(MKFW) --prototype $(IMAGE_DIR)/$(BOARD).prototype \
		--git_identity $(PX4_BASE) \
		--parameter_xml $(PRODUCT_PARAMXML) \
		--image $< > $@
else
	$(Q) $(MKFW) --prototype $(IMAGE_DIR)/$(BOARD).prototype \
		--git_identity $(PX4_BASE) \
		--image $< > $@
endif

$(PRODUCT_BIN):		$(PRODUCT_ELF)
	$(call SYM_TO_BIN,$<,$@)

$(PRODUCT_ELF):		$(OBJS) $(MODULE_OBJS) $(LIBRARY_LIBS) $(GLOBAL_DEPS) $(LINK_DEPS) $(MODULE_MKFILES)
	$(call LINK,$@,$(OBJS) $(MODULE_OBJS) $(LIBRARY_LIBS))

#
# Utility rules
#

.PHONY: upload
upload:	$(PRODUCT_BUNDLE) $(PRODUCT_BIN)
	$(Q) $(MAKE) -f $(PX4_MK_DIR)/upload.mk \
		METHOD=serial \
		CONFIG=$(CONFIG) \
		BOARD=$(BOARD) \
		BUNDLE=$(PRODUCT_BUNDLE) \
		BIN=$(PRODUCT_BIN)

.PHONY: clean
clean:			$(MODULE_CLEANS)
	@$(ECHO) %% cleaning
	$(Q) $(REMOVE) $(PRODUCT_BUNDLE) $(PRODUCT_BIN) $(PRODUCT_ELF)
	$(Q) $(REMOVE) $(OBJS) $(DEP_INCLUDES) $(EXTRA_CLEANS)
	$(Q) $(RMDIR) $(NUTTX_EXPORT_DIR)

