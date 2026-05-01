# Copyright 2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# Configuration file for macos-setup.sh and env.sh
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
    arm64)
        ;;
    *)
        echo "Unsupported architecture: $MORSE_ARCH"
        exit 1
        ;;
esac
