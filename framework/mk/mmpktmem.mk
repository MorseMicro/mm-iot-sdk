#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MMPKTMEM_DIR = src/mmpktmem

# Default values for pktmem type, TX and RX pool sizes, if not otherwise specified
MMPKTMEM_TYPE ?= heap

MMIOT_SRCS_C += $(MMPKTMEM_DIR)/mmpktmem_$(MMPKTMEM_TYPE).c

ifneq ($(MMPKTMEM_TX_POOL_N_BLOCKS),)
BUILD_DEFINES += MMPKTMEM_TX_POOL_N_BLOCKS=$(MMPKTMEM_TX_POOL_N_BLOCKS)
endif

ifneq ($(MMPKTMEM_RX_POOL_N_BLOCKS),)
BUILD_DEFINES += MMPKTMEM_RX_POOL_N_BLOCKS=$(MMPKTMEM_RX_POOL_N_BLOCKS)
endif
