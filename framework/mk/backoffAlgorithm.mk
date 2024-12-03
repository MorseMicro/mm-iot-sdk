#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

BACKOFF_ALGORITHM_DIR = src/freertos-libs/backoffAlgorithm/source

# C files to be included while compiling
BACKOFF_ALGORITHM_SRCS_C += backoff_algorithm.c

# Header files for dependancy checking
BACKOFF_ALGORITHM_SRCS_H += include/backoff_algorithm.h

MMIOT_SRCS_C += $(addprefix $(BACKOFF_ALGORITHM_DIR)/,$(BACKOFF_ALGORITHM_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(BACKOFF_ALGORITHM_DIR)/,$(BACKOFF_ALGORITHM_SRCS_H))

MMIOT_INCLUDES += $(BACKOFF_ALGORITHM_DIR)/include
