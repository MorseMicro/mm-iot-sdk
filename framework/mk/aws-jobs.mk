#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_JOBS_DIR = src/aws/AWS-Jobs/source

# C files to be included while compiling
AWS_JOBS_SRCS_C += jobs.c

# Header files for dependancy checking
AWS_JOBS_SRCS_H += include/jobs.h

MMIOT_SRCS_C += $(addprefix $(AWS_JOBS_DIR)/,$(AWS_JOBS_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_JOBS_DIR)/,$(AWS_JOBS_SRCS_H))

MMIOT_INCLUDES += $(AWS_JOBS_DIR)/include
