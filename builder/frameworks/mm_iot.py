#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
import logging
import os

from SCons.Script import DefaultEnvironment
from utils import (
    erase_configstore,
    import_mm_iot_components,
    import_morse_fw_bin,
    program_configstore,
)

env = DefaultEnvironment()
platform = env.PioPlatform()
board = env.BoardConfig()
env_dict = env.Dictionary()

if "COUNTRY_CODE" in os.environ:
    logging.info("Using COUNTRY_CODE from environment: %s", os.environ["COUNTRY_CODE"])
    env.Append(CCFLAGS=f"-DCOUNTRY_CODE={os.environ['COUNTRY_CODE']}")


BOARD_NAME = env_dict["BOARD"]

FRAMEWORK_DIR = os.path.join(platform.get_dir(), "framework")
FRAMEWORK_SRC_DIR = os.path.join(FRAMEWORK_DIR, "src")
BUILD_DIR = os.path.join(env_dict["PROJECT_BUILD_DIR"], BOARD_NAME)

env["ENV"]["MMIOT_ROOT"] = FRAMEWORK_DIR

components = []

DEFAULT_IP_STACK = "lwip"

#
# Add core specific flags
#
if board.get("build.core") == "stm32":
    machine_flags = [
        "-mthumb",
        f"-mcpu={board.get('build.cpu')}",
    ]

    fpu = board.get("build.fpu", default=None)
    if fpu:
        machine_flags += [
            "-mfloat-abi=hard",
            f"-mfpu={fpu}",
        ]
    env.Append(ASFLAGS=machine_flags, CCFLAGS=machine_flags, LINKFLAGS=machine_flags)

#
# Additional flags
#
env.Append(
    CCFLAGS=[
        "-ffunction-sections",
        "-fdata-sections",
        "-specs=nano.specs",
    ],
    CXXFLAGS=[
        "-fno-rtti",
        "-fno-exceptions",
    ],
    CPPPATH=[
        "$PROJECT_DIR/src"
    ],
    LINKFLAGS=[
        "-Wl,--gc-sections",
    ],
    LIBS=[
        "c_nano", "m", "stdc++"
    ])


#
# Misc defines
#
components.append("misc")


#
# Add BOARD and BSP specific source files and include paths
#
components.append(f"platform-{BOARD_NAME}")


#
# morselib
#
morselib_arch = {
    "cortex-m33": "arm-cortex-m33f",
    "cortex-m4": "arm-cortex-m4f",
    "cortex-m7": "arm-cortex-m7f",
}[board.get("build.cpu")]

morselib_dir = os.path.join(FRAMEWORK_DIR, "morselib")
env.Append(
    CPPPATH=[
        os.path.join(morselib_dir, "include"),
    ],
    LIBPATH=[
        os.path.join(morselib_dir, "lib", morselib_arch),
    ],
    LIBS=[
        "morse",
    ]
)

#
# FreeRTOS
#
freertos_port = {
    "cortex-m33": "arm-cortex-m33f",
    "cortex-m4": "arm-cortex-m4f",
    "cortex-m7": "arm-cortex-m7f",
}[board.get("build.cpu")]


#
# Components from application platformio.ini
#
components += env.GetProjectOption("custom_mm_iot_components", default="").split()

# morsefirmware component is a special case -- do not import the makefile fragment
components = [component for component in components if component != "morsefirmware"]

import_morse_fw_bin(env, FRAMEWORK_DIR, "morsefirmware")

#
# Load components and update Platform IOO Environment
#
import_mm_iot_components(env, FRAMEWORK_DIR, components, env_vars={
    "CORE": freertos_port,
    "FREERTOS_HEAP_TYPE": "4",
    "MMIOT_ROOT": FRAMEWORK_DIR,
    "IP_STACK": env.GetProjectOption("custom_ip_stack", default=DEFAULT_IP_STACK),
    "BSP_LD_POSTFIX": env.GetProjectOption("bsp_ld_postfix", default=None),
    "MM_SHIM_OS_SRCS_C": env.GetProjectOption("mm_shim_os", default=None),
    "BSP_SRCS_MAIN_C": env.GetProjectOption("bsp_srcs_main", default=None),
})

#
# Config Store
#


def program_configstore_action(*args, **kwargs):
    return program_configstore(env, args, kwargs)


def erase_configstore_action(*args, **kwargs):
    return erase_configstore(env, args, kwargs)


def load_pip_dependencies(*_args, **_kwargs):
    env.Execute("$PYTHONEXE -m pip list")
    env.Execute("$PYTHONEXE -m pip install protobuf~=3.20.3 coloredlogs dict-to-dataclass hjson")


env.AddCustomTarget(
    name="program_configstore",
    dependencies=None,
    actions=[
        program_configstore_action
    ],
    title="Program configstore",
    description="Starts an OpenOCD session and programs the configuration store of config.hjson"
)
env.AddPreAction("program_configstore", load_pip_dependencies)

env.AddCustomTarget(
    name="erase_configstore",
    dependencies=None,
    actions=[
        erase_configstore_action
    ],
    title="Erase configstore",
    description="Starts an OpenOCD session and erases the configuration store partition"
)
env.AddPreAction("erase_configstore", load_pip_dependencies)
