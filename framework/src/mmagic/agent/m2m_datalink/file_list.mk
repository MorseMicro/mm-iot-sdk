#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

# Note we use := because we need immediate expansion of $(DIR)
INCLUDES := $(INCLUDES) $(DIR)

MMAGIC_DATALINK_FILES = \
	mmagic_datalink_uart.c \
	mmagic_datalink.h \
	mmagic_datalink_agent.h \
	mmagic_datalink_controller.h

MMAGIC_DATALINK_PUBLIC_HEADERS = \
	mmagic_datalink_agent.h \
	mmagic_datalink_controller.h \
	mmagic_datalink.h

DOXYGEN_COMPONENT_EXAMPLE_PATHS := $(DOXYGEN_COMPONENT_EXAMPLE_PATHS) $(DIR)

$(eval $(call add_files_to_package,$(DIR),$(PACKAGE_SUBDIR_SOURCE_CODE)/mmagic_datalink,$(MMAGIC_DATALINK_FILES)))
$(eval $(call add_files_to_package,$(DIR),$(PACKAGE_SUBDIR_MK),mmagic_datalink.mk))

DOXYGEN_INPUTS += $(addprefix $(PACKAGE_BUILD_DIR)/$(PACKAGE_SUBDIR_SOURCE_CODE)/mmagic_datalink/,$(MMAGIC_DATALINK_PUBLIC_HEADERS))
