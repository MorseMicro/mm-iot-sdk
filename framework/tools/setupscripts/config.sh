# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# Configuration file for ubuntu-setup.sh and env.sh
#

# Dry-run wrapper for piped commands
function dryrun_pipe()
{
    local command_str="$1"
    if [[ -n "$DRYRUNCMD" ]]; then
        echo "$command_str"
    else
        eval "$command_str"
    fi
}

export MORSE_TOOLS_DIR=/opt/morse

MORSE_ARCH="$(uname -m)"
case $MORSE_ARCH in
    x86_64)
        MORSE_ARCH2=x64
        ;;
    aarch64)
        MORSE_ARCH2=arm64
        ;;
    *)
        echo "Unsupported architecture: $MORSE_ARCH"
        exit 1
        ;;
esac
