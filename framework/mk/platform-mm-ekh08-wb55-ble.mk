#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE = arm-cortex-m4f
PLATFORM_PATH = src/platforms/mm-ekh08-wb55-ble

# Platform specific files
BSP_DIR = $(PLATFORM_PATH)/bsp

BSP_SRCS_MAIN_C ?= Core/Src/main.c
BSP_SRCS_C += $(BSP_SRCS_MAIN_C)
BSP_SRCS_C += Core/Src/stm32wbxx_hal_msp.c
BSP_SRCS_C += Core/Src/stm32wbxx_hal_timebase_tim.c
BSP_SRCS_C += Core/Src/stm32wbxx_it.c
BSP_SRCS_C += Core/Src/system_stm32wbxx.c

BSP_SRCS_H += Core/Inc/main.h
BSP_SRCS_H += Core/Inc/stm32_assert.h
BSP_SRCS_H += Core/Inc/stm32wbxx_hal_conf.h
BSP_SRCS_H += Core/Inc/stm32wbxx_it.h

# Driver files pulled from common source
BSP_DRIVERS_DIR = src/bsps/stm32cubewb

BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_utils.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_usart.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_rtc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_spi.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_rng.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_hsem.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_tim.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_tim_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_ipcc.c

BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32WBxx/Include/stm32wb55xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32WBxx/Include/stm32wbxx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32WBxx/Include/system_stm32wbxx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_compiler.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_gcc.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_version.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm4.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/mpu_armv7.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_exti.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_utils.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_dma.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_dmamux.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_pwr.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_rcc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_rtc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_bus.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_crs.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_system.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_cortex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_spi.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_usart.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_gpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_rng.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_hal_rtc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_hal_rtc_ex.h

# BLE specific BSP files
## BLE Specific Core files
BSP_SRCS_C += Core/Src/app_debug.c
BSP_SRCS_C += Core/Src/app_entry.c
BSP_SRCS_C += Core/Src/hw_timerserver.c
BSP_SRCS_C += Core/Src/stm32_lpm_if.c

BSP_SRCS_H += Core/Inc/app_common.h
BSP_SRCS_H += Core/Inc/app_conf.h
BSP_SRCS_H += Core/Inc/app_debug.h
BSP_SRCS_H += Core/Inc/app_entry.h
BSP_SRCS_H += Core/Inc/hw_conf.h
BSP_SRCS_H += Core/Inc/hw_if.h
BSP_SRCS_H += Core/Inc/stm32_lpm_if.h
BSP_SRCS_H += Core/Inc/utilities_conf.h

## Application specific BLE WPAN files
BSP_SRCS_C += STM32_WPAN/App/app_ble.c
BSP_SRCS_C += STM32_WPAN/App/dis_app.c
BSP_SRCS_C += STM32_WPAN/App/hrs_app.c
BSP_SRCS_C += STM32_WPAN/Target/hw_ipcc.c

BSP_SRCS_H += STM32_WPAN/App/app_ble.h
BSP_SRCS_H += STM32_WPAN/App/ble_conf.h
BSP_SRCS_H += STM32_WPAN/App/ble_dbg_conf.h
BSP_SRCS_H += STM32_WPAN/App/dis_app.h
BSP_SRCS_H += STM32_WPAN/App/hrs_app.h
BSP_SRCS_H += STM32_WPAN/App/tl_dbg_conf.h

# BLE specific Driver files
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/svc/Src/svc_ctl.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/svc/Src/hrs.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/svc/Src/dis.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/ble/core/template/osal.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/utilities/dbg_trace.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/utilities/stm_queue.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/utilities/otp.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/utilities/stm_list.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci/shci.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl_if.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/tl_mbox.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl.c
BSP_DRIVERS_SRCS_C += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl_if.c
BSP_DRIVERS_SRCS_C += Utilities/lpm/tiny_lpm/stm32_lpm.c


BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/hw.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/tl.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/mbox_def.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci/shci.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/ble.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/ble_common.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/ble_bufsize.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/ble_core.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/ble_std.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/ble_defs.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/ble_legacy.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_types.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/auto/ble_events.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/template/ble_const.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/template/compiler.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/core/template/osal.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/bas.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/bls.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/crs_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/dis.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/eds_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/hids.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/hrs.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/hts.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/ias.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/lls.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/tps.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/bas.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/motenv_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/hrs.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/dis.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/zdd_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/otas_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/mesh.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/template_stm.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/svc_ctl.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Inc/uuid.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/ble/svc/Src/common_blesvc.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/utilities/dbg_trace.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/utilities/utilities_common.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/utilities/stm_queue.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/utilities/otp.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/utilities/stm_list.h
BSP_DRIVERS_SRCS_H += Middlewares/ST/STM32_WPAN/stm32_wpan_common.h
BSP_DRIVERS_SRCS_H += Utilities/lpm/tiny_lpm/stm32_lpm.h


BSP_SRCS_S += startup_stm32wb55xx_cm4.s
BSP_LD_PREFIX = stm32wb55xx_flash_cm4

MM_SHIM_DIR     = $(PLATFORM_PATH)/mm_shims
MM_SHIM_SRCS_C += $(patsubst $(MMIOT_ROOT)/$(MM_SHIM_DIR)/%,%,$(wildcard $(MMIOT_ROOT)/$(MM_SHIM_DIR)/*.c))
MM_SHIM_SRCS_H += mmport.h
MM_SHIM_SRCS_C := $(filter-out mmosal_shim_freertos.c mmosal_shim_libc_stubs.c,$(MM_SHIM_SRCS_C))
MM_SHIM_OS_SRCS_C ?= mmosal_shim_freertos.c mmosal_shim_libc_stubs.c
MM_SHIM_SRCS_C += $(MM_SHIM_OS_SRCS_C)
MM_SHIM_SRCS_H += endian.h

MMIOT_SRCS_C += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_C))
MMIOT_SRCS_C += $(addprefix $(BSP_DRIVERS_DIR)/,$(BSP_DRIVERS_SRCS_C))
MMIOT_SRCS_C += $(addprefix $(MM_SHIM_DIR)/,$(MM_SHIM_SRCS_C))
MMIOT_SRCS_S += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_S))
MMIOT_SRCS_H += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_H))
MMIOT_SRCS_H += $(addprefix $(BSP_DRIVERS_DIR)/,$(BSP_DRIVERS_SRCS_H))
MMIOT_SRCS_H += $(addprefix $(MM_SHIM_DIR)/,$(MM_SHIM_SRCS_H))

MMIOT_INCLUDES += $(BSP_DIR)/Core/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/STM32WBxx_HAL_Driver/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Include
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32WBxx/Include
MMIOT_INCLUDES += $(MM_SHIM_DIR)

# BLE specific includes
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/ble
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/utilities
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/ble/core
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/ble/core/auto
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/ble/core/template
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN/ble/svc/Inc
MMIOT_INCLUDES += $(BSP_DIR)/STM32_WPAN/App
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Middlewares/ST/STM32_WPAN
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Utilities/sequencer
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Utilities/lpm/tiny_lpm


BUILD_DEFINES += USE_HAL_DRIVER
BUILD_DEFINES += STM32WB55xx
BUILD_DEFINES += USE_FULL_LL_DRIVER

ifneq ($(ENABLE_EXT_XTAL_INIT),)
BUILD_DEFINES += ENABLE_EXT_XTAL_INIT=$(ENABLE_EXT_XTAL_INIT)
endif

CONLYFLAGS += -include $(MMIOT_ROOT)/$(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32WBxx/Include/stm32wb55xx.h
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-c++-compat
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-unused-parameter
CFLAGS-$(BSP_DIR) += -Wno-unused-function
CFLAGS-$(BSP_DIR) += -Wno-unused-variable
CFLAGS-$(BSP_DIR) += -Wno-unused-parameter
CFLAGS-$(BSP_DIR) += -Wno-missing-field-initializers

LINKFLAGS += --entry Reset_Handler
LINKFLAGS += -Wl,--defsym,HEAPSIZE=0x1000
LINKFLAGS += -Wl,--defsym,STACKSIZE=0x1000
LINKFLAGS += -Wl,--defsym,_CSTACK_Base=0x00000000
LINKFLAGS += -Wl,--defsym,_CSTACK_Length=0x1000
LINKFLAGS += -Wl,--undefined=uxTopUsedPriority
LINKFLAGS += -Wl,--print-memory-usage

BSP_LD_POSTFIX ?= .ld
BSP_LD_FILES += $(BSP_DIR)/$(BSP_LD_PREFIX)$(BSP_LD_POSTFIX)
