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

source $SCRIPT_DIR/config.sh || return 1

# Source setup.d scripts for variable definitions only (no install logic).
VARS_ONLY=1
source $SCRIPT_DIR/setup.d/S09_gcc-arm-none-eabi-14 || return 1
source $SCRIPT_DIR/setup.d/S20_openocd || return 1
unset VARS_ONLY

# Set MMIOT_ROOT environment variable to the framework directory that this script resides under.
export MMIOT_ROOT="$(realpath $SCRIPT_DIR/../..)"

MM_RET=0

# Add GNU utilities to the front of PATH so that standard tool names (make, sed, etc.) resolve to
# the GNU versions rather than the BSD versions shipped with macOS. This is required for build
# system compatibility with Linux. This only affects the current shell session.
HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
GNU_PACKAGES=(coreutils gnu-sed gnu-tar make)
for pkg in "${GNU_PACKAGES[@]}"; do
    GNUBIN="$HOMEBREW_PREFIX/opt/$pkg/libexec/gnubin"
    if [[ -d "$GNUBIN" ]] && [[ ":$PATH:" != *":${GNUBIN}:"* ]]; then
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
        MM_RET=1
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
        MM_RET=1
    fi
fi
unset CURRENT_ARM_TOOLCHAIN_DIR
unset MORSE_ARM_TOOLCHAIN_DIR

# Some tools need to be managed by symlink directly
MORSE_BIN_DIR=$MORSE_TOOLS_DIR/bin
if [[ ":$PATH:" != *":${MORSE_BIN_DIR}:"* ]]; then
    if [ -d "$MORSE_BIN_DIR" ]; then
        echo "Adding Morse symlinks directory to the path: $MORSE_BIN_DIR"
        export PATH=$MORSE_BIN_DIR:$PATH
    else
        echo "Morse symlinks directory not found at $MORSE_BIN_DIR"
        echo "Please run $SCRIPT_DIR/macos-setup.sh S02_brew-pkg"
        MM_RET=1
    fi
fi
unset MORSE_BIN_DIR SCRIPT_DIR

if [[ "$MM_RET" -ne 0 ]]; then
    return $MM_RET
fi
unset MM_RET

# MM-IoT-SDK Version
export MMIOT_VERSION="2.14.3"
