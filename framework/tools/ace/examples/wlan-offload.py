#!/usr/bin/env python3
#
# Copyright 2022-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
WLAN Offload
"""

import argparse
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, ip_addr_to_bytes  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666


def _handle_dhcp_offload(args, dutif):  # noqa: ARG001
    dutif.exec("wlan/enable_dhcp_offload")
    print("DHCP Offload enabled")


def _handle_arp_offload(args, dutif):
    arp_addr = ip_addr_to_bytes(args.arp_addr)
    u32_arp_addr = arp_addr[0] + (arp_addr[1] << 8) + (arp_addr[2] << 16) + (arp_addr[3] << 24)
    dutif.exec("wlan/enable_arp_response_offload", arp_addr=u32_arp_addr)
    print("ARP response offload successful")


def _handle_arp_refresh(args, dutif):
    dest_ip = ip_addr_to_bytes(args.dest_ip)
    u32_dest_ip = dest_ip[0] + (dest_ip[1] << 8) + (dest_ip[2] << 16) + (dest_ip[3] << 24)
    dutif.exec("wlan/enable_arp_refresh_offload", interval_s=args.refresh_s,
               dest_ip=u32_dest_ip, send_as_garp=args.send_as_garp)
    print("ARP refresh offload successful")


def _handle_enable_tcp_keepalive(args, dutif):
    dutif.exec("wlan/enable_tcp_keepalive_offload", period_s=args.period_s,
               retry_count=args.retry_count, retry_interval_s=args.retry_interval_s)
    print("TCP keepalive enabled")


def _handle_disable_tcp_keepalive(_args, dutif):
    dutif.exec("wlan/disable_tcp_keepalive_offload")
    print("TCP keepalive disabled")


def _handle_set_whitelist_filter(args, dutif):
    ip_addr = ip_addr_to_bytes(args.src_ip)
    src_ip = ip_addr[0] + (ip_addr[1] << 8) + (ip_addr[2] << 16) + (ip_addr[3] << 24)
    ip_addr = ip_addr_to_bytes(args.dst_ip)
    dst_ip = ip_addr[0] + (ip_addr[1] << 8) + (ip_addr[2] << 16) + (ip_addr[3] << 24)
    ip_addr = ip_addr_to_bytes(args.netmask)
    netmask = ip_addr[0] + (ip_addr[1] << 8) + (ip_addr[2] << 16) + (ip_addr[3] << 24)

    dutif.exec("wlan/set_whitelist_filter",
               src_ip=src_ip,
               dst_ip=dst_ip,
               netmask=netmask,
               src_port=args.src_port,
               dst_port=args.dst_port,
               ip_protocol=args.ip_protocol,
               llc_protocol=args.llc_protocol)
    print("IP whitelist filter enabled")


def _handle_clear_whitelist_filter(_args, dutif):
    dutif.exec("wlan/clear_whitelist_filter")
    print("IP whitelist filter disabled")


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

    subparsers = parser.add_subparsers(help="sub-command help", required=True,
                                       dest="command")

    parser_arp_offload = subparsers.add_parser("arp_offload",
                                               help="Enable the ARP offload feature.")
    parser_arp_offload.add_argument("arp_addr", help="Specifies the IP address to advertise in ARP")
    parser_arp_offload.set_defaults(func=_handle_arp_offload)

    parser_arp_refresh = subparsers.add_parser("arp_refresh",
                                               help="Enable the ARP refresh feature.")
    parser_arp_refresh.add_argument("refresh_s", type=int,
                                    help="Specifies ARP refresh interval in seconds")
    parser_arp_refresh.add_argument("dest_ip", type=str, default="192.168.1.1",
                                    help="The target IP to send periodic ARP refreshes to.")
    parser_arp_refresh.add_argument("send_as_garp", default=True, type=bool,
                                    help="send as gratuitous arp, if true the dest_ip is ignored.")
    parser_arp_refresh.set_defaults(func=_handle_arp_refresh)

    parser_dhcp_offload = subparsers.add_parser("dhcp_offload",
                                                help="Enable the DHCP offload feature.")
    parser_dhcp_offload.set_defaults(func=_handle_dhcp_offload)

    parser_tcp_keepalive = subparsers.add_parser("enable_tcp_keepalive",
                                                 help="Enable the TCP keepalive offload feature.")
    parser_tcp_keepalive.add_argument("period_s", type=int,
                                      help="Specifies TCP keepalive period in seconds.")
    parser_tcp_keepalive.add_argument("retry_count", type=int,
                                      help="Specifies number of times to retry before giving up.")
    parser_tcp_keepalive.add_argument("retry_interval_s", type=int,
                                      help="Specifies retry interval in seconds")
    parser_tcp_keepalive.set_defaults(func=_handle_enable_tcp_keepalive)

    parser_disable_tcp_keepalive = subparsers.add_parser("disable_tcp_keepalive",
                                                         help="Disables TCP keepalive feature.")
    parser_disable_tcp_keepalive.set_defaults(func=_handle_disable_tcp_keepalive)

    parser_set_ip_whitelist = subparsers.add_parser("set_ip_whitelist",
                                                    help="Setup IP whitelist filtering.")
    parser_set_ip_whitelist.add_argument("src_ip", type=str, default="0.0.0.0",
                                         help="Specifies the source IP to filter on.")
    parser_set_ip_whitelist.add_argument("dst_ip", type=str, default="0.0.0.0",
                                         help="Specifies the destination IP to filter on.")
    parser_set_ip_whitelist.add_argument("netmask", type=str, default="0.0.0.0",
                                         help="Specifies the netmask to apply to above IP's.")
    parser_set_ip_whitelist.add_argument("src_port", type=int, default=0,
                                         help="Specifies the source port to filter on.")
    parser_set_ip_whitelist.add_argument("dst_port", type=int, default=0,
                                         help="Specifies the destination port to filter on.")
    parser_set_ip_whitelist.add_argument("ip_protocol", type=int, default=0,
                                         help="Specifies the IP protocol to filter on.")
    parser_set_ip_whitelist.add_argument("llc_protocol", type=int, default=0,
                                         help="Specifies the LLC protocol to filter on.")
    parser_set_ip_whitelist.set_defaults(func=_handle_set_whitelist_filter)

    parser_clear_ip_whitelist = subparsers.add_parser("clear_ip_whitelist",
                                                      help="Disables IP whitelist filtering.")
    parser_clear_ip_whitelist.set_defaults(func=_handle_clear_whitelist_filter)

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    try:
        args.func(args, dutif)
    except AttributeError as e:
        print("You likely forgot to specify a sub-command.")
        print(e)
        sys.exit(1)


if __name__ == "__main__":
    _main()
