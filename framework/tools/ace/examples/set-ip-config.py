#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace set ip config example
"""

import argparse
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, ip_addr_bytes_to_str, ip_addr_to_bytes  # noqa: E402

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
    parser.add_argument("-D", "--enable-dhcp", action="store_true",
                        help='Enables DHCP. If not specified then a static IP address, netmask, and gateway address must be specified.')    # noqa
    parser.add_argument("-I", "--ip-address",
                        help="Local IP address of the device to set")
    parser.add_argument("-N", "--netmask",
                        help="Netmask of the device to set")
    parser.add_argument("-G", "--gateway-address",
                        help="Gateway address to set")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if not args.enable_dhcp:
        if not (args.ip_address and args.netmask and args.gateway_address):
            print("IP Address, Netmask, and Gateway Address must be specified if DHCP is not enabled")  # noqa: E501
            sys.exit(1)

    ip_addr = ip_addr_to_bytes(args.ip_address) if args.ip_address else b"\0\0\0\0"
    netmask = ip_addr_to_bytes(args.netmask) if args.netmask else b"\0\0\0\0"
    gw_addr = ip_addr_to_bytes(args.gateway_address) if args.gateway_address else b"\0\0\0\0"

    dutif.exec("lwip/set_ip_config",
               dhcp_enabled=args.enable_dhcp,
               ip_addr=ip_addr,
               netmask=netmask,
               gateway_addr=gw_addr)

    print(f'DHCP {"enabled" if args.enable_dhcp else "disabled"}')
    print(f"IP: {ip_addr_bytes_to_str(ip_addr)}, Netmask: {ip_addr_bytes_to_str(netmask)}, Gateway: {ip_addr_bytes_to_str(gw_addr)}")    # noqa: E501


if __name__ == "__main__":
    _main()
