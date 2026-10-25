#
# Copyright 2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

EMMET_DIR = src/emmet

EMMET_SRCS_C += emmet_lwip.c
EMMET_SRCS_C += emmet_wlan.c
EMMET_SRCS_C += emmet_sys.c
EMMET_SRCS_C += emmet_capi.c
EMMET_SRCS_C += transport_mem.c
EMMET_SRCS_C += protobuf-c/protobuf-c.c
EMMET_SRCS_C += emmet_cfg.c
EMMET_SRCS_C += emmet_hal.c
EMMET_SRCS_C += emmet.c
EMMET_SRCS_C += emmet_file.c
EMMET_SRCS_C += emmet_lwip_ipv6addr.c
EMMET_SRCS_C += emmet-api.pb-c.c

EMMET_SRCS_H += emmet_private.h
EMMET_SRCS_H += protobuf-c/protobuf-c.h
EMMET_SRCS_H += emmet_hal.c
EMMET_SRCS_H += emmet.h
EMMET_SRCS_H += emmet-api.pb-c.h

MMIOT_SRCS_C += $(addprefix $(EMMET_DIR)/,$(EMMET_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(EMMET_DIR)/,$(EMMET_SRCS_H))

# Internal morselib dependencies
MMIOT_INCLUDES += morselib/src

MMIOT_INCLUDES += $(EMMET_DIR)

CFLAGS-$(EMMET_DIR)/protobuf-c += -Wno-c++-compat
