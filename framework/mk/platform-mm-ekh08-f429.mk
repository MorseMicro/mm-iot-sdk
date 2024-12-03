#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE = arm-cortex-m4f
PLATFORM_PATH = src/platforms/mm-ekh08-f429

# Platform specific files
BSP_DIR = $(PLATFORM_PATH)/bsp

BSP_SRCS_MAIN_C ?= Core/src/bsp.c
BSP_SRCS_C += $(BSP_SRCS_MAIN_C)
BSP_SRCS_C += Core/src/stm32f4xx_it.c
BSP_SRCS_C += Core/src/stm32f4xx_hal_msp.c
BSP_SRCS_C += Core/src/system_stm32f4xx.c

BSP_SRCS_H += Core/Inc/bsp.h
BSP_SRCS_H += Core/Inc/stm32_assert.h
BSP_SRCS_H += Core/Inc/stm32f4xx_hal_conf.h
BSP_SRCS_H += Core/Inc/stm32f4xx_it.h

# Driver files pulled from common source
BSP_DRIVERS_DIR = src/bsps/stm32cubef4

BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rtc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_utils.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_spi.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usart.c

BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f429xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f4xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32F4xx/Include/system_stm32f4xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_compiler.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_gcc.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_version.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm4.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/mpu_armv7.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_def.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ramfunc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_cortex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_exti.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usb.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rcc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_utils.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_exti.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_spi.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_gpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_dma.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_bus.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_system.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_cortex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_pwr.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rng.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rtc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_utils.h

BSP_SRCS_S += startup_stm32f429xx.s
BSP_LD_PREFIX = STM32F429ZITx_FLASH

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

MMIOT_INCLUDES += $(BSP_DIR)
MMIOT_INCLUDES += $(BSP_DIR)/Core/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/STM32F4xx_HAL_Driver/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Include
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx/Include
MMIOT_INCLUDES += $(MM_SHIM_DIR)

BUILD_DEFINES += USE_HAL_DRIVER
BUILD_DEFINES += STM32F429xx
BUILD_DEFINES += STM32F4_HAL
BUILD_DEFINES += USE_FULL_LL_DRIVER

ifneq ($(ENABLE_EXT_XTAL_INIT),)
BUILD_DEFINES += ENABLE_EXT_XTAL_INIT=$(ENABLE_EXT_XTAL_INIT)
endif

CONLYFLAGS += -include $(MMIOT_ROOT)/$(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f429xx.h
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-c++-compat
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-unused-parameter
# Due to unused parameter in stm32f4xx_ll_rtc.h
CFLAGS-$(MM_SHIM_DIR) += -Wno-unused-parameter
CFLAGS-$(BSP_DIR) += -Wno-unused-parameter


LINKFLAGS += --entry Reset_Handler
LINKFLAGS += -Wl,--defsym,HEAPSIZE=0x1000
LINKFLAGS += -Wl,--defsym,STACKSIZE=0x1000
LINKFLAGS += -Wl,--defsym,_CSTACK_Base=0x00000000
LINKFLAGS += -Wl,--defsym,_CSTACK_Length=0x1000
LINKFLAGS += -Wl,--undefined=uxTopUsedPriority
LINKFLAGS += -Wl,--print-memory-usage

BSP_LD_POSTFIX ?= .ld
BSP_LD_FILES += $(BSP_DIR)/$(BSP_LD_PREFIX)$(BSP_LD_POSTFIX)
