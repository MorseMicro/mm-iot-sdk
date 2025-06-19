#!/usr/bin/env python3
#
# Copyright 2025 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace set default qos queue params example.
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


def _list_from_aci_arg(arg, aci):
    try:
        vals = [aci]
        vals += arg.split(",")
        assert(len(vals) == 5)
        return vals
    except AssertionError:
        raise ValueError(f"Error parsing values for aci values {arg}")


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
    parser.add_argument("-a0", "--aci0-args", default="3,15,1023,15008",
                        help="Set the default args for aci0. Values should be comma separated. \
                            In order, these are aifs, cw_min, cw_max and txop_max_us.")
    parser.add_argument("-a1", "--aci1-args", default="7,15,1023,15008",
                        help="Set the default args for aci1. Values should be comma separated. \
                            In order, these are aifs, cw_min, cw_max and txop_max_us.")
    parser.add_argument("-a2", "--aci2-args", default="2,7,15,15008",
                        help="Set the default args for aci2. Values should be comma separated. \
                            In order, these are aifs, cw_min, cw_max and txop_max_us.")
    parser.add_argument("-a3", "--aci3-args", default="2,3,7,15008",
                        help="Set the default args for aci3. Values should be comma separated. \
                            In order, these are aifs, cw_min, cw_max and txop_max_us.")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)
    try:
        for aci, aci_args in enumerate([args.aci0_args, args.aci1_args, args.aci2_args,
                                        args.aci3_args]):
            aifs, cw_min, cw_max, txop_max_us = aci_args.split(",")
            dutif.exec("wlan/set_default_qos_queue_params", aci=aci, aifs=aifs, cw_min=cw_min,
                       cw_max=cw_max, txop_max_us=txop_max_us)

        print("Set default qos queue params")

    except Exception as e:
        print(f"Command execution failed at aci {aci}, returned error {e}")


if __name__ == "__main__":
    _main()
