#!/usr/bin/env python3
#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace Firmware update example
"""

import argparse
import hashlib
import logging
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf  # noqa: E402
from acelib.dutif import CommandExecutionError  # noqa: E402

FILE_CHUNK_SIZE = 1024

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

    parser.add_subparsers(help="sub-command help", dest="command")
    parser.add_argument("-f", "--file", required=True,
                        help="MBIN file to load")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    # Generate SHA256 hash of MBIN file
    hash = hashlib.new("sha256")
    with open(args.file, "rb") as f:
        hash.update(f.read())
    dutif.exec("config/write", key="IMAGE_SIGNATURE", payload=hash.digest())

    devicefile = "update.mbin"
    with open(args.file, "rb") as f:
        # Delete any existing file before starting
        try:
            dutif.exec("file/del", path=devicefile)
        except CommandExecutionError:
            pass
        while True:
            # Read file in chunks and send to device
            buffer = f.read(FILE_CHUNK_SIZE)
            if not buffer:
                break
            dutif.exec("file/write", path=devicefile, payload=buffer)
    logging.info("File transferred successfully")

    # Write UPDATE_IMAGE key
    dutif.exec("config/write", key="UPDATE_IMAGE", payload=bytes(devicefile + "\0", "utf-8"))

    # Reset device to trigger loader
    dutif.reset()


if __name__ == "__main__":
    _main()
