#!/bin/bash -e
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
#
# Script to install toolchain and packages for building the Morse Micro IoT SDK
#
# If the --dry-run parameter is passed then do not actually take any action, just print the
# commands that this script (and its sub scripts) would run.
#
# Tested on Ubuntu 22.04 x86_64
# Tested on Ubuntu 22.04 arm64/aarch64
#

# Trap and print a loud error message if something goes wrong
function ErrorTrapHandler()
{
    local status=$?
    echo -e "\033[31mAn error occurred and the requested operation did not complete\033[0m "
    exit $status
}
trap 'ErrorTrapHandler' ERR


export SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
if command -v sudo >/dev/null 2>&1
then
    export SUDO="${SUDO:-sudo}"
else
    export SUDO="${SUDO:-}"
fi

TARGET_SUBSCRIPT=""
while [[ $# -gt 0 ]]
do
    case "$1" in
        --dry-run)
            echo -e "\033[1mDry run\033[0m ... will only echo commands that would be run"
            export DRYRUNCMD=echo
            shift
            ;;
        *)
            if [[ -n "$TARGET_SUBSCRIPT" ]]
            then
                echo "Unknown argument: $1"
                exit 1
            fi
            TARGET_SUBSCRIPT="$1"
            shift
            ;;
    esac
done

if [[ -t 0 ]]
then
    echo
    echo -e "\033[1mDISCLAIMER\033[0m

    This script is provided \"as is\" without warranty of any kind, either
    expressed or implied and is to be used at your own risk. This script requires
    super user access. Back up data before executing this script.

    This script will download and install software (Third-Party Software) from
    third parties sources. Morse Micro makes no warranty regarding Third-Party
    Software and shall have no liability or obligation arising therefrom. It is
    your responsibility to verify the trustworthiness of any third-party sources.
    "

    read -p $'Press \u001b[1mCTRL-C\u001b[0m to abort or \u001b[1mENTER\u001b[0m to continue...'
else
    echo -e "\033[33mNon-interactive mode; skipping confirmation prompt.\033[0m"
fi

source $SCRIPT_DIR/config.sh

$DRYRUNCMD $SUDO mkdir -p $MORSE_TOOLS_DIR

pushd /tmp > /dev/null

echo
SUBSCRIPTS=("$SCRIPT_DIR"/setup.d/S*)
if [[ -n "$TARGET_SUBSCRIPT" ]]
then
    case "$TARGET_SUBSCRIPT" in
        setup.d/*) TARGET_PATH="$SCRIPT_DIR/$TARGET_SUBSCRIPT" ;;
        *)         TARGET_PATH="$SCRIPT_DIR/setup.d/$TARGET_SUBSCRIPT" ;;
    esac

    if [[ ! -f "$TARGET_PATH" ]]
    then
        echo "Subscript not found: $TARGET_PATH"
        exit 1
    fi

    SUBSCRIPTS=("$TARGET_PATH")
    echo -e "\033[1mRunning single subscript:\033[0m $TARGET_PATH"
fi

for subscript in "${SUBSCRIPTS[@]}"
do
    /bin/bash -e $subscript
    echo
done

popd > /dev/null

if [[ -n "$TARGET_SUBSCRIPT" ]]
then
    echo -e "
\u001b[32m\033[1mComplete\033[0m\033[0m

Ran subscript: $TARGET_SUBSCRIPT
"
else
    /bin/bash -e $SCRIPT_DIR/post-setup.sh
fi

