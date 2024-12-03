#!/usr/bin/env python3
#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace Config store example
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

    subparsers.add_parser("erase", help="Erases existing keys from config store")

    parser_write_string = subparsers.add_parser("write-string",
                                                help="Write a single string entry to mmconfig")
    parser_write_string.add_argument("key", help="The key in configstore to load the value to")
    parser_write_string.add_argument("value",
                                     help="The string value to write to the specified key")

    parser_delete_key = subparsers.add_parser("delete-key",
                                              help="Deletes the specified key only")
    parser_delete_key.add_argument("key", help="The key in configstore to delete")

    parser_read_string = subparsers.add_parser("read-string",
                                               help="Reads the specified key as a string")
    parser_read_string.add_argument("key", help="The key in configstore to read")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if args.command == "write-string":
        # MMCONFIG requires NULL terminator to be explicitly added to string data
        value = args.value + "\0"
        rsp = dutif.exec("config/write", key=args.key, payload=bytes(value, "utf-8"))
        logging.info("Key/Value programmed successfully")

    elif args.command == "read-string":
        rsp = dutif.exec("config/read", key=args.key)
        print(rsp)
        if (rsp.len >= 0):
            print(rsp.payload)
        else:
            logging.error("Error reading config store")

    elif args.command == "delete-key":
        rsp = dutif.exec("config/del", key=args.key)
        logging.info("Key deleted successfully")

    elif args.command == "erase":
        rsp = dutif.exec("config/erase")
        logging.info("Config store erased successfully")


if __name__ == "__main__":
    _main()
