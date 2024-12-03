#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# This Makefile fragment adds support for C++ exceptions
CSPECS = -specs="nosys.specs"
BUILD_DEFINES += LIBC_PROVIDES__EXIT=1
LINKER = $(CXX)
