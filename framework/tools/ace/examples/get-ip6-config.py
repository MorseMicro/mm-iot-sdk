#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace get ip config example
"""

import argparse
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, ip_addr_bytes_to_str  # noqa: E402

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

    rsp = dutif.exec("lwip/get_ip6_config")
    if rsp.mode is None or rsp.mode == 0:
        print("IPv6 Static Address mode")
    elif rsp.mode == 1:
        print("IPv6 Stateless Address Autoconfiguration mode")
    elif rsp.mode == 2:
        print("IPv6 Stateless DHCPv6 with SLAAC")

    if rsp.addrs is not None:
        for i in range(len(rsp.addrs)):
            print(f"IPv6[{i}] : {ip_addr_bytes_to_str(rsp.addrs[i])}")


if __name__ == "__main__":
    _main()
