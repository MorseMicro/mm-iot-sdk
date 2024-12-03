#!/usr/bin/env python3
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace echo example
"""

import argparse
import os
import sys
import time

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

    parser.add_argument("-l", "--load-elf",
                        help="Specifies the path to an optional elf file to be loaded")
    parser.add_argument("-r", "--reset", action="store_true",
                        help="Reset the device before proceeding "
                             "(implied if --load-elf is specified)")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)
    from acelib.dutif import print_commands
    print_commands()

    if args.load_elf:
        dutif.load(args.load_elf)
        dutif.reset()
        time.sleep(5)
    elif args.reset:
        dutif.reset()
        time.sleep(5)

    rsp = dutif.exec("sys/echo", payload="Echo test", intpayload=42)
    print(f"Echo response: {rsp}")
    print(f"    payload={rsp.payload}")
    print(f"    intpayload={rsp.intpayload}")


if __name__ == "__main__":
    _main()
