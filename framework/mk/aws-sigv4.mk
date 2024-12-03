#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_SIGV4_DIR = src/aws/AWS-SigV4/source

# C files to be included while compiling
AWS_SIGV4_SRCS_C += sigv4.c
AWS_SIGV4_SRCS_C += sigv4_quicksort.c

# Header files for dependancy checking
AWS_SIGV4_SRCS_H += include/sigv4.h
AWS_SIGV4_SRCS_H += include/sigv4_internal.h
AWS_SIGV4_SRCS_H += include/sigv4_quicksort.h
AWS_SIGV4_SRCS_H += include/sigv4_config_defaults.h

MMIOT_SRCS_C += $(addprefix $(AWS_SIGV4_DIR)/,$(AWS_SIGV4_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_SIGV4_DIR)/,$(AWS_SIGV4_SRCS_H))

MMIOT_INCLUDES += $(AWS_SIGV4_DIR)/include
