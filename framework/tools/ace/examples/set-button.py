#!/usr/bin/env python3
#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace set button example. Sets specified button state, waits, then sets to a different state.
"""
help_epilog = "Example discrete button press: -1 BUTTON_PRESSED -d 0.2 -2 BUTTON_RELEASED"

import argparse
import os
import sys
import time

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
                        help="ID of the button to manipulate")
    parser.add_argument("-1", "--state1", default="BUTTON_PRESSED",
                        help="Initial state to set: BUTTON_PRESSED or BUTTON_RELEASED")
    parser.add_argument("-d", "--duration", default="0.2", type=float,
                        help="Duration in secs (floating point accepted) before set of final state")
    parser.add_argument("-2", "--state2", default="BUTTON_RELEASED",
                        help="Final state to set: BUTTON_PRESSED, BUTTON_RELEASED or None to skip")

    args = parser.parse_args()

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)
    dutif.exec("hal/set_button", button_id=args.buttonid, button_state=args.state1)

    if args.state2.casefold() != "None".casefold():
        time.sleep(float(args.duration))
        dutif.exec("hal/set_button", button_id=args.buttonid, button_state=args.state2)


if __name__ == "__main__":
    _main()
