#
# Copyright 2022-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE := arm-cortex-m33f

# Platform specific files
BSP_DIR = $(APP_DIR)/bsp

BSP_SRCS_MAIN_C ?= Core/Src/main.c
BSP_SRCS_C += $(BSP_SRCS_MAIN_C)
BSP_SRCS_C += Core/Src/stm32u5xx_hal_msp.c
BSP_SRCS_C += Core/Src/stm32u5xx_hal_timebase_tim.c
BSP_SRCS_C += Core/Src/stm32u5xx_it.c
BSP_SRCS_C += Core/Src/system_stm32u5xx.c

BSP_SRCS_H += Core/Inc/main.h
BSP_SRCS_H += Core/Inc/stm32_assert.h
BSP_SRCS_H += Core/Inc/stm32u5xx_hal_conf.h
BSP_SRCS_H += Core/Inc/stm32u5xx_it.h

# Driver files pulled from common source
BSP_DRIVERS_DIR = src/bsps/stm32cubeu5

BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gtzc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_icache.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rng.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rng_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi_ex.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_dma.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_exti.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_gpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_lpgpio.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_lptim.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_rcc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_rtc.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_spi.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_utils.c
BSP_DRIVERS_SRCS_C += Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_usart.c

BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u575xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u5xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Device/ST/STM32U5xx/Include/system_stm32u5xx.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_armcc.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_armclang.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_armclang_ltm.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_compiler.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_gcc.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_iccarm.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/cmsis_version.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_armv81mml.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_armv8mbl.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_armv8mml.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm0.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm0plus.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm1.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm23.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm33.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm35p.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm3.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm4.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_cm7.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_sc000.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/core_sc300.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/mpu_armv7.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/mpu_armv8.h
BSP_DRIVERS_SRCS_H += Drivers/CMSIS/Include/tz_context.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_cortex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_def.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_dma_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_dma.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_exti.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_flash_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_flash.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_gpio_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_gpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_gtzc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_i2c_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_i2c.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_icache.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_pwr_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_pwr.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_rcc_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_rcc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_rng_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_rng.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_tim_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_tim.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_spi.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_spi_ex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_bus.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_cortex.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_crs.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_dma.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_exti.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_gpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_icache.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_lpgpio.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_lptim.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_pwr.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_rcc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_rng.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_rtc.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_spi.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_system.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_usart.h
BSP_DRIVERS_SRCS_H += Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_ll_utils.h

BSP_SRCS_S += startup_stm32u575zitxq.s
BSP_LD_PREFIX = STM32U575ZITXQ_FLASH

MM_SHIM_DIR     = $(APP_DIR)/mm_shims
MM_SHIM_SRCS_C += $(patsubst $(MM_SHIM_DIR)/%,%,$(wildcard $(MM_SHIM_DIR)/*.c))

MM_SHIM_SRCS_H += mmport.h
MM_SHIM_SRCS_C := $(filter-out mmosal_shim_freertos.c,$(MM_SHIM_SRCS_C))
MM_SHIM_OS_SRCS_C ?= mmosal_shim_freertos.c
MM_SHIM_SRCS_C += $(MM_SHIM_OS_SRCS_C)
MM_SHIM_SRCS_H += endian.h

SRCS_C += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_C))
SRCS_C += $(addprefix $(MM_SHIM_DIR)/,$(MM_SHIM_SRCS_C))
SRCS_S += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_S))
SRCS_H += $(addprefix $(BSP_DIR)/,$(BSP_SRCS_H))
SRCS_H += $(addprefix $(MM_SHIM_DIR)/,$(MM_SHIM_SRCS_H))

MMIOT_SRCS_C += $(addprefix $(BSP_DRIVERS_DIR)/,$(BSP_DRIVERS_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(BSP_DRIVERS_DIR)/,$(BSP_DRIVERS_SRCS_H))

INCLUDES += $(BSP_DIR)/Core/Inc
INCLUDES += $(MM_SHIM_DIR)

MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/STM32U5xx_HAL_Driver/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Include
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32U5xx/Include

BUILD_DEFINES += USE_HAL_DRIVER
BUILD_DEFINES += STM32U575xx
BUILD_DEFINES += STM32U5_HAL
BUILD_DEFINES += USE_FULL_LL_DRIVER

BUILD_DEFINES += configUSE_TICKLESS_IDLE=1
ENABLE_DEBUG_IN_STOP_MODE ?= 1
BUILD_DEFINES += ENABLE_DEBUG_IN_STOP_MODE=$(ENABLE_DEBUG_IN_STOP_MODE)

# Enable log output through the ITM/SWO port
DEBUG_BUILD_DEFINES += ENABLE_ITM_LOG

CONLYFLAGS += -include $(MMIOT_ROOT)/$(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u575xx.h
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
LD_FILES += $(BSP_DIR)/$(BSP_LD_PREFIX)$(BSP_LD_POSTFIX)
