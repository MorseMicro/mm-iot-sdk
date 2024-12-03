#!/usr/bin/env python3
#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace get button example. Retrieves the current state of the specified button, if available.
"""
help_epilog = "Output in the format: BUTTON_ID_USER0 = BUTTON_PRESSED"

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
                                     description=__doc__, epilog=help_epilog)
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="Increase verbosity of log messages (repeat for increased verbosity)")
    parser.add_argument("-H", "--debug-host", required=True,
                        help="Hostname of machine on which OpenOCD is running")
    parser.add_argument("-g", "--gdb-port", default=DEFAULT_GDB_PORT, type=int,
                        help="GDB port to use")
    parser.add_argument("-t", "--tcl-port", default=DEFAULT_TCL_PORT, type=int,
                        help="OpenOCD TCL port to use")
    parser.add_argument("-i", "--buttonid", default="BUTTON_ID_USER0",
                        help="ID of the button state to retrieve")
    parser.add_argument("-f", "--follow", action="store_true",
                        help="Keep updating the output until terminated")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    while True:
        rsp = dutif.exec("hal/get_button", button_id=args.buttonid)

        if rsp.button_state == 1:
            sys.stdout.write(f"\r{args.buttonid} = BUTTON_PRESSED ")
        else:
            sys.stdout.write(f"\r{args.buttonid} = BUTTON_RELEASED")
        sys.stdout.flush()

        if not args.follow:
            break

        try:
            time.sleep(1)
        except KeyboardInterrupt:
            break
    print("\n")


if __name__ == "__main__":
    _main()
