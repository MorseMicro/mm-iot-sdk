#!/usr/bin/env python3
"""
Emmet/Ace set dwell time example.

Sets the per-channel dwell time to use for scans that are requested internally within the
mmwlan driver (e.g., when connecting or background scanning).
"""
#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
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

    parser.add_argument("-d", "--dwell-time-ms", type=int, default=105,
                        help="The dwell time to set (in milliseconds)")
    parser.add_argument("-e", "--ndp-probe-enabled", action=argparse.BooleanOptionalAction,
                        default=False, help="Enable NDP probe support")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    dutif.exec("wlan/set_scan_config", dwell_time_ms=args.dwell_time_ms,
               ndp_probe_enabled=args.ndp_probe_enabled)
    status = "enabled" if args.ndp_probe_enabled else "disabled"
    print(f"Scan dwell time set to {args.dwell_time_ms} ms.")
    print(f"NDP Probe support {status}")


if __name__ == "__main__":
    _main()
