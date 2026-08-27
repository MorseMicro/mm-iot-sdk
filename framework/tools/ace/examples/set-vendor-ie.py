#!/usr/bin/env python3
#
# Copyright 2024-2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace example: add and clear custom vendor IEs on the DUT.
"""

import argparse
import logging
import os
import sys

import morse_common

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import ACE_ENUM, DutIf  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

IE_LENGTH_MIN = 3
IE_LENGTH_MAX = 255


def add_ie(dutif, vendor_ie, types):
    try:
        ie = bytes.fromhex(vendor_ie)
    except ValueError:
        logging.error("Vendor IE is not a valid hex string")
        raise

    if len(ie) < IE_LENGTH_MIN or len(ie) > IE_LENGTH_MAX:
        logging.error(
            f"Vendor IE length {len(ie)} out of range "
            f"[{IE_LENGTH_MIN}, {IE_LENGTH_MAX}] bytes."
        )
        sys.exit(1)

    logging.info(f"Adding vendor IE (types={types}): {ie.hex()}")
    dutif.exec("wlan/vendor_ie_add_ie", ie=ie, mgmt_types=types)


def clear_ies(dutif, types):
    logging.info(f"Clearing vendor IEs (types={types})")
    dutif.exec("wlan/vendor_ie_clear_ies", mgmt_types=types)


def _main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description=__doc__,
    )
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="Increase log verbosity (repeat for more)")
    parser.add_argument("-H", "--debug-host", required=True,
                        help="Hostname of machine on which OpenOCD is running")
    parser.add_argument("-g", "--gdb-port", default=DEFAULT_GDB_PORT, type=int,
                        help="GDB port to use")
    parser.add_argument("-t", "--tcl-port", default=DEFAULT_TCL_PORT, type=int,
                        help="OpenOCD TCL port to use")
    parser.add_argument("-f", "--frame-types", nargs="+", required=True,
                        choices=ACE_ENUM.VendorIeMgmtType.choices(),
                        metavar="FRAME_TYPE",
                        help=f"Frame types to apply. "
                             f"Choices: {list(ACE_ENUM.VendorIeMgmtType.choices())}")

    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("-a", "--add", metavar="VENDOR_IE",
                        help="Add a vendor element (hex string, format: [OUI][payload])")
    action.add_argument("-c", "--clear", action="store_true",
                        help="Clear previously added vendor elements.")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if args.clear:
        clear_ies(dutif, args.frame_types)
    else:
        add_ie(dutif, args.add, args.frame_types)


if __name__ == "__main__":
    _main()
