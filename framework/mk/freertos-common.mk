#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

FREERTOS_HEAP_TYPE ?= 4
BUILD_DEFINES += HEAP_$(FREERTOS_HEAP_TYPE)

FREERTOS_COMMON_DIR = src/freertos

FREERTOS_COMMON_SRCS_C += stream_buffer.c
FREERTOS_COMMON_SRCS_C += queue.c
FREERTOS_COMMON_SRCS_C += tasks.c
FREERTOS_COMMON_SRCS_C += event_groups.c
FREERTOS_COMMON_SRCS_C += croutine.c
FREERTOS_COMMON_SRCS_C += list.c
FREERTOS_COMMON_SRCS_C += timers.c
FREERTOS_COMMON_SRCS_C += mmiot/heap_$(FREERTOS_HEAP_TYPE)_mm.c
FREERTOS_COMMON_SRCS_C += mmiot/hooks.c

FREERTOS_COMMON_SRCS_H += include/mpu_prototypes.h
FREERTOS_COMMON_SRCS_H += include/task.h
FREERTOS_COMMON_SRCS_H += include/list.h
FREERTOS_COMMON_SRCS_H += include/queue.h
FREERTOS_COMMON_SRCS_H += include/FreeRTOS.h
FREERTOS_COMMON_SRCS_H += include/projdefs.h
FREERTOS_COMMON_SRCS_H += include/deprecated_definitions.h
FREERTOS_COMMON_SRCS_H += include/portable.h
FREERTOS_COMMON_SRCS_H += include/croutine.h
FREERTOS_COMMON_SRCS_H += include/stack_macros.h
FREERTOS_COMMON_SRCS_H += include/event_groups.h
FREERTOS_COMMON_SRCS_H += include/semphr.h
FREERTOS_COMMON_SRCS_H += include/mpu_wrappers.h
FREERTOS_COMMON_SRCS_H += include/message_buffer.h
FREERTOS_COMMON_SRCS_H += include/stream_buffer.h
FREERTOS_COMMON_SRCS_H += include/timers.h
FREERTOS_COMMON_SRCS_H += mmiot/include/FreeRTOSConfig.h
FREERTOS_COMMON_SRCS_H += mmiot/include/heap_mm.h

MMIOT_SRCS_C += $(addprefix $(FREERTOS_COMMON_DIR)/,$(FREERTOS_COMMON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(FREERTOS_COMMON_DIR)/,$(FREERTOS_COMMON_SRCS_H))

MMIOT_INCLUDES += $(FREERTOS_COMMON_DIR)/include $(FREERTOS_COMMON_DIR)/mmiot/include

CFLAGS-$(FREERTOS_COMMON_DIR) += -Wno-c++-compat

# Enable queue registry. This allow for RTOS objects like queues and sempahores viewed if you are
# using a RTOS kernel aware debugger. https://www.freertos.org/vQueueAddToRegistry.html
DEBUG_BUILD_DEFINES += configQUEUE_REGISTRY_SIZE=15

# Panic on malloc failure
DEBUG_BUILD_DEFINES += configUSE_MALLOC_FAILED_HOOK=1

# Enable FreeRTOS integrity checks to help catch cases of data structures getting trampled on
#DEBUG_BUILD_DEFINES += configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES=1
