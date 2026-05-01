# Copyright 2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
# Internal-only script to help user add required tool paths to shell run commands (.*shrc) file
#

if [[ -z "$SCRIPT_DIR" ]]; then
    echo -e "\e[31mScript should be run from setup bash script\e[0m" >&2
    exit 1
fi

echo -e "
\033[32m\033[1mComplete\033[0m\033[0m

Tools have been installed in $MORSE_TOOLS_DIR

Please source $SCRIPT_DIR/env.sh to setup your environment. For example:

    . $SCRIPT_DIR/env.sh

You can add the above line to your ~/.bashrc or ~/.zshrc (depending on which shell you use) for a
more permanent solution. You may want to direct the output to /dev/null in this case.
"
