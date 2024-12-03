#!/usr/bin/env python3
#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace File I/O example
"""

import argparse
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

    subparsers = parser.add_subparsers(help="sub-command help",
                                       dest="command")

    parser_write = subparsers.add_parser("write",
                                         help="Copy a file from the host to the device")
    parser_write.add_argument("source", help="The source path on the host")
    parser_write.add_argument("destination", help="The destination path on the device")

    parser_delete = subparsers.add_parser("delete",
                                          help="Deletes the specified file on the device")
    parser_delete.add_argument("target", help="The file on the device to delete")

    parser_read = subparsers.add_parser("read",
                                        help="Copy a file from the device to the host")
    parser_read.add_argument("source", help="The source path on the device")
    parser_read.add_argument("destination", help="The destination path on the host")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if args.command == "write":
        with open(args.source, "rb") as f:
            # Delete any existing file before starting
            try:
                dutif.exec("file/del", path=args.destination)
            except CommandExecutionError:
                print("No file deleted")
            while True:
                # Read file in chunks and send to device
                buffer = f.read(FILE_CHUNK_SIZE)
                if not buffer:
                    break
                rsp = dutif.exec("file/write", path=args.destination, payload=buffer)
        logging.info("File transferred successfully")

    elif args.command == "read":
        with open(args.destination, "wb") as f:
            offset = 0
            while True:
                rsp = dutif.exec("file/read", path=args.source, offset=offset, len=64)
                if not rsp.len or rsp.len < 0:
                    break
                f.write(rsp.payload)
                offset += rsp.len
        logging.info("File transferred successfully")

    elif args.command == "delete":
        rsp = dutif.exec("file/del", path=args.target)
        logging.info("File deleted successfully")


if __name__ == "__main__":
    _main()
