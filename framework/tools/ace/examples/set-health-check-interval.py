#!/usr/bin/env python3
"""
Emmet/Ace set periodic health check interval example.

Allows specifying the minimum and maximum intervals for periodic health checks.
If enabled, will attempt to schedule a health check at the first available opportunity
after the minimum interval has passed and will force a health check if the maximum interval
elapses since the last health check. Health checks are disabled if both intervals are set to 0.
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

    parser.add_argument("-m", "--min-interval-ms", type=int, required=True,
                        help="The minimum health check interval (in milliseconds)")
    parser.add_argument("-x", "--max-interval-ms", type=int, required=True,
                        help="The maximum health check interval (in milliseconds)")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    dutif.exec("wlan/set_health_check_interval",
               min_health_check_intvl_ms=args.min_interval_ms,
               max_health_check_intvl_ms=args.max_interval_ms)
    print(f"Health check interval set to range {args.min_interval_ms}..{args.max_interval_ms} ms")


if __name__ == "__main__":
    _main()
