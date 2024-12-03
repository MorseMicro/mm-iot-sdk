#
# Copyright 2021-2023 Morse Micro
#

#
# This file is not used by PlatformIO. It is only used for building using make.
#

APP_NAME = twt_setup

PLATFORM = mm-mm6108-ekh05-sdio


# Path to root of the MM IoT SDK package. This will need to be overriden  with the path to
# the unzipped MM IoT SDK if it is moved from its current location.
MMIOT_ROOT ?= ../../../../framework

# Name of directory in which to put build outputs and intermediate files
BUILD_DIR ?= build

# Names of executable files to generate
ELF_FILE = $(BUILD_DIR)/$(APP_NAME).elf
BIN_FILE = $(BUILD_DIR)/$(APP_NAME).bin
MBIN_FILE = $(BUILD_DIR)/$(APP_NAME).mbin

# Set VERBOSE env var to get more verbose output
ifeq ($(VERBOSE),)
QUIET = @
endif

# Set BUILD_SUPPLICANT_FROM_SOURCE to build the WPA Supplicant from source and link
# against libmorse_nosupplicant.a. If this is set, the crypto will also be built from source
# using mbedTLS.
# Note that this will take precedence over BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE if both are set.
#BUILD_SUPPLICANT_FROM_SOURCE=y

# Set BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE to build the WPA Supplicant crypto from source
# using mbedTLS.
# against libmorse_nosupplicant.a. If not set then the WPA Supplicant will be linked
# as part of libmorse.a
#BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE=y

# Example source files
APP_DIR = ../..
SRCS_C = $(wildcard $(APP_DIR)/src/*.c)
SRCS_CPP = $(wildcard $(APP_DIR)/src/*.cpp)
SRCS_H = $(wildcard $(APP_DIR)/src/*.h)

INCLUDES += $(APP_DIR)/src





# Strict C flags (may be overridden for third-party code as required)
CFLAGS += -Werror -Wall -Wextra
CONLYFLAGS += -Wc++-compat


# This determines the network stack that will be built.
# Options:
# - lwip
# - freertosplustcp
IP_STACK ?= lwip

# This enables/disables IPv4 support.
# Options:
# - 0
# - 1
MMIPAL_IPV4_ENABLED ?= 1

# This enables/disables IPv6 support.
# Options:
# - 0
# - 1
MMIPAL_IPV6_ENABLED ?= 1


# Ensure all is the first rule
all:

# Import Makefile fragments for the various components
-include $(MMIOT_ROOT)/mk/misc.mk
include $(MMIOT_ROOT)/mk/utils.mk

include $(MMIOT_ROOT)/mk/platform-$(PLATFORM).mk    # Note this provides CORE

include $(MMIOT_ROOT)/mk/core-$(CORE).mk

include $(MMIOT_ROOT)/mk/mmconfig.mk
include $(MMIOT_ROOT)/mk/slip.mk
include $(MMIOT_ROOT)/mk/mmutils.mk
include $(MMIOT_ROOT)/mk/littlefs.mk
include $(MMIOT_ROOT)/mk/freertos-common.mk
include $(MMIOT_ROOT)/mk/freertos-$(CORE).mk
include $(MMIOT_ROOT)/mk/morselib.mk
include $(MMIOT_ROOT)/mk/morsefirmware.mk
include $(MMIOT_ROOT)/mk/wpa_supplicant.mk
include $(MMIOT_ROOT)/mk/mbedtls.mk
include $(MMIOT_ROOT)/mk/mmipal.mk
include $(MMIOT_ROOT)/mk/mmpktmem.mk
include $(MMIOT_ROOT)/mk/$(IP_STACK).mk


SRCS_H += $(addprefix $(MMIOT_ROOT)/,$(MMIOT_SRCS_H))
# A quick sanity check to see if we're missing any files that the Make targets depend on.
$(eval $(call check_for_files,$(SRCS_H)))

OBJS += $(patsubst $(APP_DIR)/%.s $(APP_DIR)/%.S,$(BUILD_DIR)/%.o,$(SRCS_S))
OBJS += $(patsubst $(APP_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C))
OBJS += $(patsubst $(APP_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS_CPP))
OBJS += $(patsubst %.c,$(BUILD_DIR)/%.o,$(MMIOT_SRCS_C))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.s,$(BUILD_DIR)/%.o,$(MMIOT_SRCS_S)))

LIBS += $(addprefix $(MMIOT_ROOT)/,$(MMIOT_LIBS))

CFLAGS += $(addprefix -I,$(INCLUDES))
CFLAGS += $(addprefix -I$(MMIOT_ROOT)/,$(MMIOT_INCLUDES))
CFLAGS += $(addprefix -D,$(BUILD_DEFINES))
CFLAGS += $(CSPECS)

# Add a build define with the hash of the filename (MMOSAL_FILEID).
ifeq ($(DISABLE_FILEID_GENERATION),)
FILEHASHES_LIST=$(BUILD_DIR)/filehashes.txt
CFLAGS += -DMMOSAL_FILEID=$(call uint32_hash,$<)
endif

LINKER ?= $(CC)

LINKFLAG_PREFIX=-Wl,-T
LD_FILES += $(addprefix $(MMIOT_ROOT)/,$(BSP_LD_FILES))
LINKFLAGS += $(addprefix $(LINKFLAG_PREFIX),$(LD_FILES))

ELF_TO_MBIN = $(MMIOT_ROOT)/tools/buildsystem/convert-bin-to-mbin.py

-include $(APP_DIR)/app.mk

.PHONY: all
all: $(ELF_FILE)
bin: $(BIN_FILE)
mbin: $(MBIN_FILE)


.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f $(ELF_FILE)



#
# Rules to compile/assemble sources in $(MMIOT_ROOT)
#
$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.s $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(AS) -o $@ -c $(CFLAGS) $<

$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.S $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(AS) -o $@ -c $(CFLAGS) $<

$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.c $(SRCS_H)
	@echo "$(MSGPFX)Compiling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(call file_cflags,$(patsubst $(MMIOT_ROOT)/%,%,$<)) $<

#
# Rules to compile/assemble sources in $(APP_DIR)
#
$(BUILD_DIR)/%.o: $(APP_DIR)/%.s $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(APP_CFLAGS) $<

$(BUILD_DIR)/%.o: $(APP_DIR)/%.S $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(APP_CFLAGS) $<

$(BUILD_DIR)/%.o: $(APP_DIR)/%.c $(SRCS_H)
	@echo "$(MSGPFX)Compiling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(APP_CFLAGS) $<

$(BUILD_DIR)/%.o: $(APP_DIR)/%.cpp $(SRCS_H)
	@echo "$(MSGPFX)Compiling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CXX) -o $@ -c $(CFLAGS) $(CPPFLAGS) $(APP_CFLAGS) $<


#
# Rules to link the executable and convert to other binary formats
#
$(ELF_FILE): $(OBJS) $(LIBS) $(LD_FILES) $(FILEHASHES_LIST)
	@echo "$(MSGPFX)Linking $@"
	$(QUIET)$(LINKER) -o $@ $(CFLAGS) $(LINKFLAGS) $(OBJS) $(LIBS)

$(BIN_FILE): $(ELF_FILE)
	@echo "$(MSGPFX)Generating $@"
	$(QUIET)$(OBJCOPY) -Obinary $< $@

$(MBIN_FILE): $(ELF_FILE)
	@echo Convert $< to $@
	$(QUIET)$(ELF_TO_MBIN) -s -o $@ $<

ifeq ($(DISABLE_FILEID_GENERATION),)
# Generate a file contain a list of the file hashes
$(FILEHASHES_LIST): $(SRCS_C) $(addprefix $(MMIOT_ROOT)/,$(MMIOT_SRCS_C))
	@mkdir -p $(dir $@)
	@bash -c 'echo -e $(foreach src,$^,$(call uint32_hash,$(src)) $(src)\\n)' > $@
endif