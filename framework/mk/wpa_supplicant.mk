#
# Copyright 2022-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

SUPP_DIR ?= src/hostap


#
# Configuration options
#
# ************************************************************************
# *                             Warning                                  *
# ************************************************************************
# * Modifying the configuration below is not recommended. The MM IoT SDK *
# * WPA Supplicant driver has only been tested with the supplied         *
# * configuration. Modifying it may result in compilation failures or    *
# * undefined behaviour.                                                 *
# ************************************************************************
#
BUILD_DEFINES += IEEE8021X_EAPOL=1
BUILD_DEFINES += CONFIG_SME=1
BUILD_DEFINES += CONFIG_SAE=1
BUILD_DEFINES += CONFIG_OWE=1
BUILD_DEFINES += CONFIG_ECC=1
BUILD_DEFINES += CONFIG_SHA256=1
BUILD_DEFINES += CONFIG_SHA512=1
BUILD_DEFINES += CONFIG_SHA384=1
BUILD_DEFINES += CONFIG_FIPS=1
BUILD_DEFINES += CONFIG_NO_BSS_TRANS_MGMT=1
BUILD_DEFINES += CONFIG_NO_CONFIG_BLOBS=1
BUILD_DEFINES += CONFIG_NO_RC4=1
BUILD_DEFINES += CONFIG_NO_RRM=1
BUILD_DEFINES += CONFIG_WNM=1
BUILD_DEFINES += CONFIG_IEEE80211AH=1
BUILD_DEFINES += CONFIG_NO_CONFIG_WRITE=1
BUILD_DEFINES += OS_NO_C_LIB_DEFINES=1
BUILD_DEFINES += CONFIG_BGSCAN=1
BUILD_DEFINES += CONFIG_BGSCAN_SIMPLE=1
BUILD_DEFINES += CONFIG_S1G_TWT=1
BUILD_DEFINES += MAX_NUM_MLD_LINKS=1
BUILD_DEFINES += MAX_NUM_MLO_LINKS=1
BUILD_DEFINES += MM_IOT


ifeq ($(WPA_SUPPLICANT_ENABLE_STDOUT_DEBUG),)
BUILD_DEFINES += CONFIG_NO_WPA_MSG=1
BUILD_DEFINES += CONFIG_NO_STDOUT_DEBUG=1
endif

SUPP_INCLUDES += .
SUPP_INCLUDES += src
SUPP_INCLUDES += src/common
SUPP_INCLUDES += src/utils
SUPP_INCLUDES += wpa_supplicant

ifneq ($(BUILD_SUPPLICANT_FROM_SOURCE),)

#
# Source files and paths
#

SUPP_SRCS_C += drivers_morse.c
SUPP_SRCS_C += os_mmosal.c
SUPP_SRCS_C += src/common/dragonfly.c
SUPP_SRCS_C += src/common/hw_features_common.c
SUPP_SRCS_C += src/common/ieee802_11_common.c
SUPP_SRCS_C += src/common/ptksa_cache.c
SUPP_SRCS_C += src/common/sae.c
SUPP_SRCS_C += src/common/wpa_common.c
SUPP_SRCS_C += src/crypto/aes-unwrap.c
SUPP_SRCS_C += src/crypto/dh_groups.c
SUPP_SRCS_C += src/crypto/sha256-kdf.c
SUPP_SRCS_C += src/crypto/sha384-kdf.c
SUPP_SRCS_C += src/crypto/tls_none.c
SUPP_SRCS_C += src/crypto/sha512-kdf.c
SUPP_SRCS_C += src/drivers/driver_common.c
SUPP_SRCS_C += src/eap_common/eap_common.c
SUPP_SRCS_C += src/eap_peer/eap.c
SUPP_SRCS_C += src/eap_peer/eap_methods.c
SUPP_SRCS_C += src/eapol_supp/eapol_supp_sm.c
SUPP_SRCS_C += src/rsn_supp/preauth.c
SUPP_SRCS_C += src/rsn_supp/pmksa_cache.c
SUPP_SRCS_C += src/rsn_supp/wpa.c
SUPP_SRCS_C += src/rsn_supp/wpa_ie.c
SUPP_SRCS_C += src/utils/bitfield.c
SUPP_SRCS_C += src/utils/common.c
SUPP_SRCS_C += src/utils/wpa_debug.c
SUPP_SRCS_C += src/utils/wpabuf.c
SUPP_SRCS_C += src/utils/morse.c
SUPP_SRCS_C += wpa_supplicant/bgscan.c
SUPP_SRCS_C += wpa_supplicant/bgscan_simple.c
SUPP_SRCS_C += wpa_supplicant/bss.c
SUPP_SRCS_C += wpa_supplicant/config.c
SUPP_SRCS_C += wpa_supplicant/eap_register.c
SUPP_SRCS_C += wpa_supplicant/events.c
SUPP_SRCS_C += wpa_supplicant/notify.c
SUPP_SRCS_C += wpa_supplicant/op_classes.c
SUPP_SRCS_C += wpa_supplicant/scan.c
SUPP_SRCS_C += wpa_supplicant/sme.c
SUPP_SRCS_C += wpa_supplicant/wmm_ac.c
SUPP_SRCS_C += wpa_supplicant/wpa_supplicant.c
SUPP_SRCS_C += wpa_supplicant/wpas_glue.c
SUPP_SRCS_C += wpa_supplicant/robust_av.c
SUPP_SRCS_C += wpa_supplicant/bssid_ignore.c
SUPP_SRCS_C += wpa_supplicant/wnm_sta.c

SUPP_INCLUDES += .
SUPP_INCLUDES += src
SUPP_INCLUDES += src/common
SUPP_INCLUDES += src/utils
SUPP_INCLUDES += wpa_supplicant

MMIOT_INCLUDES += $(addprefix $(SUPP_DIR)/,$(SUPP_INCLUDES))
MMIOT_SRCS_C += $(addprefix $(SUPP_DIR)/,$(SUPP_SRCS_C))

CFLAGS-$(SUPP_DIR) += -Wno-c++-compat
CFLAGS-$(SUPP_DIR) += -Wno-unused-but-set-variable
CFLAGS-$(SUPP_DIR) += -Wno-unused-function
CFLAGS-$(SUPP_DIR) += -Wno-unused-parameter
CFLAGS-$(SUPP_DIR) += -Wno-unused-variable
CFLAGS-$(SUPP_DIR) += -Wno-format
CFLAGS-$(SUPP_DIR) += -Wno-maybe-uninitialized
CFLAGS-$(SUPP_DIR) += -Wno-dangling-else
CFLAGS-$(SUPP_DIR) += -Wno-type-limits
CFLAGS-$(SUPP_DIR) += -Wno-sign-compare

endif

ifneq ($(BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE)$(BUILD_SUPPLICANT_FROM_SOURCE),)
# mbedTLS crypto shim layer.
MMIOT_SRCS_C += $(addprefix $(SUPP_DIR)/,crypto_mbedtls_mm.c)
endif
