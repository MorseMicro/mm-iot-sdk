#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

# Configure the toolchain
TOOLCHAIN_VERSION ?= 10.3-2021.07

# Try to find the toolchain if not already specified
ifeq ($(TOOLCHAIN_DIR),)
    directory_exists = $(shell [ -d $(1) ] && echo "exists")
    TOOLCHAIN_DIR := /opt/morse/gcc-arm-none-eabi-$(TOOLCHAIN_VERSION)
    ifeq ($(call directory_exists,$(TOOLCHAIN_DIR)),)
        TOOLCHAIN_DIR := /opt/gcc-arm-none-eabi-$(TOOLCHAIN_VERSION)
        ifeq ($(call directory_exists,$(TOOLCHAIN_DIR)),)
            $(error Unable to find arm-none-eabi-$(TOOLCHAIN_VERSION) toolchain)
        endif
    endif
else
	TOOLCHAIN_DIR := $(TOOLCHAIN_DIR)
endif

TOOLCHAIN_BASE := $(TOOLCHAIN_DIR)/bin/arm-none-eabi-

CC := "$(TOOLCHAIN_BASE)gcc"
CXX := "$(TOOLCHAIN_BASE)g++"
AS := $(CC) -x assembler-with-cpp
OBJCOPY := "$(TOOLCHAIN_BASE)objcopy"
AR := "$(TOOLCHAIN_BASE)ar"
LD := "$(TOOLCHAIN_BASE)ld"

ARCH := ARMV7E-M
BFDNAME := elf32-littlearm

TOOLCHAIN_INCLUDES := "$(TOOLCHAIN_DIR)/arm-none-eabi/include"
INCLUDES += $(TOOLCHAIN_INCLUDES)

CFLAGS += -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)"
CSPECS ?= -specs="nano.specs" -lc_nano

# Enable function sections and data sections so unused can be garbage collected
CFLAGS += -ffunction-sections -fdata-sections

# Disable RWX warnings for GNU Binutils 2.39 or later. This is done to keep the linker script
# compatible with older versions. The solution when using newer versions is to add the appropriate
# output section type attributes in the linker script,
# https://sourceware.org/binutils/docs-2.39/ld/Output-Section-Attributes.html.
LINKFLAGS += $(shell $(LD) --no-warn-rwx-segments -v > /dev/null 2>&1 && echo -Wl,--no-warn-rwx-segments)

# Garbage collect sections
LINKFLAGS += -Wl,--gc-sections
