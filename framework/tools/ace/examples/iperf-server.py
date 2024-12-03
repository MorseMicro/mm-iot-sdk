#!/usr/bin/env python3
#
# Copyright 2021-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace iperf server example

The default behaviour is to start the iperf server and then continually poll for results.
This behaviour can be modified with the --start and --results flags.
"""

import argparse
import logging
import os
import sys
import time

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import ACE_ENUM, DutIf, ip_addr_bytes_to_str, ip_addr_to_bytes  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666
DEFAULT_SERVER_PORT = 5001

DEFAULT_IP_ADDR = "0.0.0.0"

DEFAULT_AMOUNT = -10 * 100  # 10 seconds

STATS_POLL_PERIOD = 2   # seconds


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

    parser.add_argument("-u", "--udp", action="store_true",
                        help="Perform a UDP iperf (defaults to TCP if not specified)")
    parser.add_argument("-B", "--bind", default=DEFAULT_IP_ADDR,
                        help="""
        An IP address to bind to. 0.0.0.0 to use the device’s configured unicast IPv4 address
        and ::0 to use the device's configured unicast IPv6 address.
    """)

    group = parser.add_mutually_exclusive_group()
    group.add_argument("-S", "--start", action="store_true",
                       help="""
        If specified, the script will terminate immediately after starting the server
    """)
    group.add_argument("-R", "--results", action="store_true",
                       help="""
        If specified, then the script will not start iperf, it will just check if results have
        been reported and print them if so
    """)
    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    port_type = "UDP" if args.udp else "TCP"
    local_addr = ip_addr_to_bytes(args.bind)

    if not args.results:
        # Start iperf server (use default port)
        rsp = dutif.exec("lwip/iperf_server_start",
                         port_type=port_type, port=DEFAULT_SERVER_PORT, local_addr=local_addr)

        if (rsp.server_state == ACE_ENUM.IperfState.RUNNING):
            print("Iperf server already started...")
        else:
            print(f"Iperf server started (bound to {args.bind})...")

        if (args.udp):
            print("execute cmd on AP 'iperf -c <STA IP> -i 1 -u -b 20M',"
                  " or 'iperf -c <STA IPv6>%wlan0 -V -i 1 -u -b 20M' for IPv6")
        else:
            print("execute cmd on AP 'iperf -c <STA IP> -i 1',"
                  " or 'iperf -c <STA IPv6>%wlan0 -V -i 1' for IPv6")

        # If the start flag was provided then we do not want to wait for results
        if args.start:
            return

        print("Waiting for results...")
        while True:
            rsp = dutif.exec("lwip/iperf_get_stats", port_type=port_type, dir="SERVER")
            logging.debug(rsp)
            if rsp.state == ACE_ENUM.IperfState.FINISHED:
                # Results are cleared once they are read, so they will only be printed once.
                print(f"Remote Address: {ip_addr_bytes_to_str(rsp.remote_addr)}:{rsp.remote_port}")     # noqa: E501
                print(f"Local Address:  {ip_addr_bytes_to_str(rsp.local_addr)}:{rsp.local_port}")       # noqa: E501
                print(f"Iperf stats: bytes transferred: {rsp.bytes_transferred}, duration: {rsp.ms_duration} ms, bandwidth: {rsp.bandwidth_kbitpsec} kbps")     # noqa: E501
            try:
                time.sleep(STATS_POLL_PERIOD)
            except KeyboardInterrupt:
                break

    else:
        rsp = dutif.exec("lwip/iperf_get_stats", port_type=port_type, dir="SERVER")
        if rsp.state is None:
            rsp.state = 0
        if rsp.state == ACE_ENUM.IperfState.FINISHED:
            # Results are cleared once they are read, so they will only be printed once.
            print(f"Remote Address: [{ip_addr_bytes_to_str(rsp.remote_addr)}]:{rsp.remote_port}")       # noqa: E501
            print(f"Local Address:  [{ip_addr_bytes_to_str(rsp.local_addr)}]:{rsp.local_port}")         # noqa: E501
            print(f"Iperf stats: bytes transferred: {rsp.bytes_transferred}, duration: {rsp.ms_duration} ms, bandwidth: {rsp.bandwidth_kbitpsec} kbps")     # noqa: E501
        elif rsp.state == ACE_ENUM.IperfState.RUNNING:
            print("Iperf server is running but there are no test results")
        elif rsp.state == ACE_ENUM.IperfState.NOT_STARTED:
            print("Iperf server is not started")
        else:
            print(f"Iperf server in unknown state ({rsp.state})")


if __name__ == "__main__":
    _main()
