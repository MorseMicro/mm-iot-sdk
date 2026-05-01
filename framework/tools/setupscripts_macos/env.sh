# Copyright 2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# Script to set up the development environment. This assumes tools have been installed in the
# appropriate locations using the provided macos-setup.sh script.
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
    return 1
fi

source $SCRIPT_DIR/config.sh

# Source setup.d scripts for variable definitions only (no install logic).
VARS_ONLY=1
source $SCRIPT_DIR/setup.d/S09_gcc-arm-none-eabi-14
source $SCRIPT_DIR/setup.d/S20_openocd
unset VARS_ONLY

# Set MMIOT_ROOT environment variable to the framework directory that this script resides under.
export MMIOT_ROOT="$SCRIPT_DIR/../.."

unset SCRIPT_DIR

# Add GNU utilities to the front of PATH so that standard tool names (find, sed, etc.) resolve to
# the GNU versions rather than the BSD versions shipped with macOS. This is required for build
# system compatibility with Linux. This only affects the current shell session.
HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
GNU_PACKAGES=(coreutils findutils gnu-sed gnu-tar grep gawk make)
for pkg in "${GNU_PACKAGES[@]}"; do
    GNUBIN="$HOMEBREW_PREFIX/opt/$pkg/libexec/gnubin"
    if [ -d "$GNUBIN" ]; then
        export PATH="$GNUBIN:$PATH"
    fi
done
unset GNUBIN GNU_PACKAGES HOMEBREW_PREFIX pkg

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
# alias to keep things simple.
if ! command -v miniterm > /dev/null 2>&1; then
    PYSERIAL_MINITERM=$(command -v pyserial-miniterm 2>/dev/null)
    if [[ -z "$PYSERIAL_MINITERM" ]]; then
        echo "Unable to find miniterm or pyserial-miniterm. Is pyserial installed?"
        return 1
    fi
    echo "miniterm command not found, creating alias to $PYSERIAL_MINITERM"
    alias miniterm="$PYSERIAL_MINITERM"
    unset PYSERIAL_MINITERM
fi

# MM-IoT-SDK Version
export MMIOT_VERSION="2.11.2"
