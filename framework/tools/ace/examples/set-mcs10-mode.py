#!/usr/bin/env python3
#
# Copyright 2025 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace set MCS10 mode example
"""

import argparse
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666


def _main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
                                     description=__doc__)
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="Increase verbosity of log messages (repeat for increased verbosity)")
    parser.add_argument("-H", "--debug-host", required=True,
                        help="Hostname of machine on which OpenOCD is running")
    parser.add_argument("-g", "--gdb-port", default=DEFAULT_GDB_PORT, type=int,
                        help="GDB port to use")
    parser.add_argument("-t", "--tcl-port", default=DEFAULT_TCL_PORT, type=int,
                        help="OpenOCD TCL port to use")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--disabled", "-d", const="MCS10_MODE_DISABLED", action="store_const",
                       dest="mode", default="MCS10_MODE_DISABLED", help="Do not use MCS10.")
    group.add_argument("--forced", "-f", action="store_const", dest="mode",
                       const="MCS10_MODE_FORCED",
                       help="Always use MCS10 instead of MCS0 when the bandwidth is 1 MHZ.")
    group.add_argument("--auto", "-a", action="store_const", const="MCS10_MODE_AUTO", dest="mode",
                       help="Use MCS10 on retries instead of MCS0 when the bandwidth is 1 MHz.")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)
    dutif.exec("wlan/set_mcs10_mode", mode=args.mode)
    print(f"MCS10 mode set to {args.mode}")


if __name__ == "__main__":
    _main()
