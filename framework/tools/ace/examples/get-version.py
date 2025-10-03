#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace get version example
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

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    rsp = dutif.exec("wlan/get_version")
    print(f"\rMorse firmware version: {rsp.morse_fw_version}")
    print(f"\rMorselib version:       {rsp.morselib_version}")
    print(f"\rMorse chip ID:          {rsp.morse_chip_id:x}")
    print(f"\rMorse chip name:        {rsp.morse_chip_id_string}")

    rsp = dutif.exec("wlan/get_bcf_metadata")
    major = rsp.version_major if rsp.version_major is not None else 0
    minor = rsp.version_minor if rsp.version_minor is not None else 0
    patch = rsp.version_patch if rsp.version_patch is not None else 0
    print(f"\rBCF API version:        {major}.{minor}.{patch}")
    if rsp.build_version:
        print(f"\rBCF build version:      {rsp.build_version}")
    if rsp.board_desc:
        print(f"\rBCF board description:  {rsp.board_desc}")


if __name__ == "__main__":
    _main()
