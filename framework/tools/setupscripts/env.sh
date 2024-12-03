# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# Script to set up the development environment. This assumes tools have been installed in the
# appropriate locations using the provided ubuntu-setup.sh script.
#
# Run the following command to load this file:
#     . ./env.sh
#

if [ -n "$ZSH_VERSION" ]; then
    SCRIPT_DIR=$( cd -- "$( dirname -- "${(%):-%x}"     )" &> /dev/null && pwd)
elif [ -n "$BASH_VERSION" ]; then
    SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd -P)
else
    echo "Unsupported shell"
    exit 1
fi

eval $(source "$SCRIPT_DIR/config.sh";
       echo MORSE_OPENOCD_DIR="$MORSE_OPENOCD_DIR";
       echo MORSE_ARM_TOOLCHAIN_DIR="$MORSE_ARM_TOOLCHAIN_DIR")

# Set MMIOT_ROOT environment variable to the framework directory that this script resides under.
export MMIOT_ROOT="$SCRIPT_DIR/../.."

unset SCRIPT_DIR

CURRENT_OPENOCD_BIN=$(dirname $(which openocd) 2> /dev/null)
if [[ "$CURRENT_OPENOCD_BIN" != "$MORSE_OPENOCD_DIR/bin" ]]; then
    if [ -d "$MORSE_OPENOCD_DIR/bin" ]; then
        echo "Adding OpenOCD bin directory to the path: $MORSE_OPENOCD_DIR/bin"
        export PATH=$MORSE_OPENOCD_DIR/bin:$PATH
    else
        echo "OpenOCD not found at $MORSE_OPENOCD_DIR/bin"
    fi
fi
unset CURRENT_OPENOCD_BIN
unset MORSE_OPENOCD_DIR


CURRENT_ARM_TOOLCHAIN_DIR=$(dirname $(which arm-none-eabi-gcc) 2> /dev/null)
if [[ "$CURRENT_ARM_TOOLCHAIN_DIR" != "$MORSE_ARM_TOOLCHAIN_DIR/bin" ]]; then
    if [ -d "$MORSE_ARM_TOOLCHAIN_DIR/bin" ]; then
        echo "Adding ARM toolchain bin directory to the path: $MORSE_ARM_TOOLCHAIN_DIR/bin"
        export PATH=$MORSE_ARM_TOOLCHAIN_DIR/bin:$PATH
    else
        echo "ARM toolchain not found at $MORSE_ARM_TOOLCHAIN_DIR/bin"
    fi
fi
unset CURRENT_ARM_TOOLCHAIN_DIR
unset MORSE_ARM_TOOLCHAIN_DIR

# Miniterm is called pyserial-miniterm in newer installations. Create an
# alias to keep things simple
which miniterm > /dev/null
if [[ $? != 0 ]]; then
    PYSERIAL_MINITERM=`which pyserial-miniterm`
    if [[ $? != 0 ]]; then
        echo "Unable to find miniterm or pyserial-miniterm. Is pyserial installed?"
        exit 1
    fi
    echo miniterm command not found, creating alias to $PYSERIAL_MINITERM
    alias miniterm=$PYSERIAL_MINITERM
fi

# Add location of the python user site-pakages to PATH. This allows for excution of packages
# installed using pip
export PATH=$HOME/.local/bin/:$PATH

# MM-IoT-SDK Version
export MMIOT_VERSION="2.6.4"
