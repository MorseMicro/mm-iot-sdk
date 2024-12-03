#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

SRCS_H += $(wildcard $(MMIOT_DIR)/morselib/include/*.h)
MMIOT_INCLUDES += morselib/include


ifneq ($(BUILD_SUPPLICANT_FROM_SOURCE),)
MMIOT_LIBS += morselib/lib/$(CORE)/libmorse_nosupplicant.a
else
ifneq ($(BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE),)
MMIOT_LIBS += morselib/lib/$(CORE)/libmorse_nocrypto.a
else
MMIOT_LIBS += morselib/lib/$(CORE)/libmorse.a
endif
endif
