#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

TINYCBOR_DIR = src/cbor/tinycbor/src

# C files to be included while compiling
TINYCBOR_SRCS_C += cborencoder.c
TINYCBOR_SRCS_C += cborparser.c
TINYCBOR_SRCS_C += cborpretty_stdio.c
TINYCBOR_SRCS_C += cborencoder_close_container_checked.c
TINYCBOR_SRCS_C += cborparser_dup_string.c
TINYCBOR_SRCS_C += cbortojson.c
TINYCBOR_SRCS_C += cborencoder_float.c
TINYCBOR_SRCS_C += cborparser_float.c
TINYCBOR_SRCS_C += cborvalidation.c
TINYCBOR_SRCS_C += cborerrorstrings.c
TINYCBOR_SRCS_C += cborpretty.c

# Header files for dependancy checking
TINYCBOR_SRCS_H += cbor.h
TINYCBOR_SRCS_H += cborinternal_p.h
TINYCBOR_SRCS_H += cborjson.h
TINYCBOR_SRCS_H += compilersupport_p.h
TINYCBOR_SRCS_H += tinycbor-version.h
TINYCBOR_SRCS_H += utf8_p.h

MMIOT_SRCS_C += $(addprefix $(TINYCBOR_DIR)/,$(TINYCBOR_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(TINYCBOR_DIR)/,$(TINYCBOR_SRCS_H))

CFLAGS-$(TINYCBOR_DIR) += -Wno-c++-compat -Wno-error

MMIOT_INCLUDES += $(TINYCBOR_DIR)
