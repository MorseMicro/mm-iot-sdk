#!/usr/bin/env python3
#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace example using beacon vendor ie filtering
"""

import argparse
import logging
import os
import sys

import morse_common
import prettytable

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

OUI_LENGTH = 3


def add_handler(args, dutif):
    logging.info(f"Adding OUI: 0x{args.oui}")

    try:
        oui = bytes.fromhex(args.oui)
    except ValueError:
        logging.error("Invalid OUI given")
        raise

    if len(oui) != OUI_LENGTH:
        logging.error("OUI is the wrong length, expected {OUI_LENGTH} bytes.")
        raise ValueError("Invalid OUI length given.")

    dutif.exec("wlan/beacon_vendor_ie_filter_add", oui=oui)


def clear_handler(_args, dutif):
    logging.info("Clear filters")
    dutif.exec("wlan/beacon_vendor_ie_filter_clear")


def get_stats_handler(_args, dutif):
    logging.info("Retrieve filter stats")
    rsp = dutif.exec("wlan/beacon_vendor_ie_filter_get_stats")

    table = prettytable.PrettyTable()
    table.field_names = ["OUI", "Occurrences:", "Most Recent IE Data"]
    table.max_width = 40

    if rsp.stats is not None:
        for stat in rsp.stats:
            data = f"0x{stat.ie.hex()}" if len(stat.ie) != 0 else "N/A"
            table.add_row([f"0x{stat.oui.hex()}", stat.occurrences, data])

    print(table)


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

    subparsers = parser.add_subparsers(help="sub-command help",
                                       dest="command")

    parser_add = subparsers.add_parser("add", help="Adds an oui to the filter list.")
    parser_add.add_argument("oui", help="Add an OUI to the vendor IE filter (hex string)")
    parser_add.set_defaults(func=add_handler)

    parser_clear = subparsers.add_parser("clear", help="Clears the filter list.")
    parser_clear.set_defaults(func=clear_handler)

    parser_get_stats = subparsers.add_parser("get-stats",
                                             help="Retrieves the stats for the installed filters")
    parser_get_stats.set_defaults(func=get_stats_handler)

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    args.func(args, dutif)


if __name__ == "__main__":
    _main()
