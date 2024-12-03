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
    echo -e "\033[31mAn error occurred and the requested operation did not complete\033[0m "
}
trap 'ErrorTrapHandler' ERR


export SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
export SUDO=sudo

if [[ $1 == "--dry-run" ]]
then
    echo -e "\033[1mDry run\033[0m ... will only echo commands that would be run"
    export DRYRUNCMD=echo
fi

echo
echo -e "\033[1mDISCLAIMER\033[0m

This script is provided \"as is\" without warranty of any kind, either
expressed or implied and is to be used at your own risk. This script requires
super user access. Back up data before executing this script.

This script will download and install software (Third-Party Software) from
third parties sources. Morse Micro makes no warranty regarding Third-Party
Software and shall have no liability or obligition arising therefrom. It is
your responsibility to verify the trustworthiness of any third-party sources.
"

read -p $'Press \u001b[1mCTRL-C\u001b[0m to abort or \u001b[1mENTER\u001b[0m to continue...'

source $SCRIPT_DIR/config.sh

$DRYRUNCMD $SUDO mkdir -p $MORSE_TOOLS_DIR

pushd /tmp > /dev/null

echo
for subscript in $SCRIPT_DIR/setup.d/S*
do
    /bin/bash -e $subscript
    echo
done

popd > /dev/null

source $SCRIPT_DIR/config.sh

echo -e "
\u001b[32m\033[1mComplete\u001b[0m\033[0m

OpenOCD installed in $MORSE_OPENOCD_DIR
ARM toolchain installed in $MORSE_ARM_TOOLCHAIN_DIR

Please source $SCRIPT_DIR/env.sh to setup your environment. For example:

    . $SCRIPT_DIR/env.sh

You can add the above line to your ~/.bashrc or ~/zshrc (depending on which
shell you use) for a more permanent solution.
"
