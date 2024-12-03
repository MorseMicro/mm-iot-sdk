#!/usr/bin/env python3
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
List the commands provided by the Emmet API.
"""

import os
import sys

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib.dutif import print_commands  # noqa: E402


def _main():
    print_commands()


if __name__ == "__main__":
    _main()
