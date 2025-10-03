#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE = arm-cortex-m4f
PLATFORM_PATH = src/platforms/mm-ekh18-wb55

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
BSP_DRIVERS_SRCS_C += Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_lptim.c
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
BSP_DRIVERS_SRCS_H += Drivers/STM32WBxx_HAL_Driver/Inc/stm32wbxx_ll_lptim.h

BSP_SRCS_S += startup_stm32wb55xx_cm4.s
BSP_LD_PREFIX = stm32wb55xx_flash_cm4

MM_SHIM_DIR     = $(PLATFORM_PATH)/mm_shims
MM_SHIM_SRCS_C += $(patsubst $(MMIOT_ROOT)/$(MM_SHIM_DIR)/%,%,$(wildcard $(MMIOT_ROOT)/$(MM_SHIM_DIR)/*.c))
MM_SHIM_SRCS_H += mmport.h
MM_SHIM_SRCS_C := $(filter-out mmosal_shim_freertos.c mmosal_shim_libc_stubs.c,$(MM_SHIM_SRCS_C))
MM_SHIM_OS_SRCS_C ?= mmosal_shim_freertos.c mmosal_shim_libc_stubs.c
MM_SHIM_SRCS_C += $(MM_SHIM_OS_SRCS_C)

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

BUILD_DEFINES += STM32WB55xx
BUILD_DEFINES += USE_FULL_LL_DRIVER
BUILD_DEFINES += HSE_VALUE=32000000
BUILD_DEFINES += HSE_STARTUP_TIMEOUT=100
BUILD_DEFINES += LSE_STARTUP_TIMEOUT=5000
BUILD_DEFINES += LSE_VALUE=32768
BUILD_DEFINES += EXTERNAL_SAI1_CLOCK_VALUE=2097000
BUILD_DEFINES += HSI_VALUE=16000000
BUILD_DEFINES += LSI_VALUE=32000

ifneq ($(ENABLE_EXT_XTAL_INIT),)
BUILD_DEFINES += ENABLE_EXT_XTAL_INIT=$(ENABLE_EXT_XTAL_INIT)
endif

BUILD_DEFINES += configUSE_TICKLESS_IDLE=1
ENABLE_DEBUG_IN_STOP_MODE ?= 1
BUILD_DEFINES += ENABLE_DEBUG_IN_STOP_MODE=$(ENABLE_DEBUG_IN_STOP_MODE)

# Enable log output through the ITM/SWO port
DEBUG_BUILD_DEFINES += ENABLE_ITM_LOG

CONLYFLAGS += -include $(MMIOT_ROOT)/$(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32WBxx/Include/stm32wb55xx.h
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-c++-compat
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-unused-parameter

LINKFLAGS += --entry Reset_Handler
LINKFLAGS += -Wl,--defsym,HEAPSIZE=0x1000
LINKFLAGS += -Wl,--defsym,STACKSIZE=0x1000
LINKFLAGS += -Wl,--defsym,_CSTACK_Base=0x00000000
LINKFLAGS += -Wl,--defsym,_CSTACK_Length=0x1000
LINKFLAGS += -Wl,--undefined=uxTopUsedPriority
LINKFLAGS += -Wl,--print-memory-usage

BSP_LD_POSTFIX ?= .ld
BSP_LD_FILES += $(BSP_DIR)/$(BSP_LD_PREFIX)$(BSP_LD_POSTFIX)
