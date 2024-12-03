#!/usr/bin/env python3
#
# Copyright 2021-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace iperf client example

The default behaviour is to start the iperf client and then wait for results.
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

DEFAULT_SERVER_IP = "192.168.1.1"
DEFAULT_AMOUNT = -10 * 100  # 10 seconds
DEFAULT_BANDWIDTH_LIMIT = 0

MIN_EXPECTED_BW_BPS = 500e3

START_TIMEOUT_S = 5


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
    parser.add_argument("-m", "--mcs-rate", type=int,
                        help="Sets MCS rate override before beginning iperf client")
    parser.add_argument("-s", "--server", default=DEFAULT_SERVER_IP,
                        help="IP address of iperf server")
    parser.add_argument("-u", "--udp", action="store_true",
                        help="Perform a UDP iperf (defaults to TCP if not specified)")
    parser.add_argument("-b", "--bandwidth", default=DEFAULT_BANDWIDTH_LIMIT, type=int,
                        help="Limit throughput to target bandwidth (in kbps)."
                              " 0 to disable the limit")
    parser.add_argument("-i", "--interval", type=int, help="Report interval in seconds.")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-T", "--time", type=float,
                       help="Iperf duration in seconds (defaults to 10 seconds)")
    group.add_argument("-n", "--num",
                       help="Number of bytes to transmit (#[kmgKMG]), UDP only")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-S", "--start", action="store_true",
                       help="""
        If specified, the script will terminate immediately after starting the client
    """)
    group.add_argument("-R", "--results", action="store_true",
                       help="""
        If specified, then the script will not start iperf, it will just check if results have
        been reported and print them if so
    """)
    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if args.mcs_rate:
        rsp = dutif.exec("wlan/set_mcs_override", mcs=args.mcs_rate)

    remote_addr = ip_addr_to_bytes(args.server)
    port_type = "UDP" if args.udp else "TCP"
    target_bw = args.bandwidth

    amount = DEFAULT_AMOUNT
    if args.time:
        amount = -int(args.time * 100)
    elif args.num:
        MULTIPLER_VALUES = {
            "k": 1e3, "K": 1e3,
            "m": 1e6, "M": 1e6,
            "g": 1e9, "G": 1e9,
        }
        multiplier = 1
        num = args.num
        m = num[-1]
        if m in MULTIPLER_VALUES:
            multiplier = MULTIPLER_VALUES[m]
            num = num[:-1]
        amount = int(int(num) * multiplier)

    if not args.results:
        # Start iperf client
        # Use default remote port, target bandwidth, and packet size
        rsp = dutif.exec("lwip/iperf_client_start",
                         port_type=port_type,
                         remote_addr=remote_addr,
                         target_bw=target_bw,
                         amount=amount)
        if args.start:
            print("Iperf client started")
            return

        print("Iperf client started, waiting for completion...")

        # Wait for iperf to start
        timeout_s = time.time() + START_TIMEOUT_S
        while time.time() < timeout_s:
            rsp = dutif.exec("lwip/iperf_get_stats", port_type=port_type, dir="CLIENT")
            logging.debug(rsp)
            if rsp.remote_addr is not None:
                break
            time.sleep(0.1)

        end_states = [ACE_ENUM.IperfState.RUNNING, ACE_ENUM.IperfState.FINISHED]
        if rsp.remote_addr is None or rsp.state not in end_states:
            print(f"Iperf failed to start after {START_TIMEOUT_S} seconds")
            return

        print("Iperf in progress...")
        if len(rsp.remote_addr) == 4:
            print(f"Remote Address: {ip_addr_bytes_to_str(rsp.remote_addr)}:{rsp.remote_port}")
            print(f"Local Address:  {ip_addr_bytes_to_str(rsp.local_addr)}:{rsp.local_port}")
        else:
            print(f"Remote Address: [{ip_addr_bytes_to_str(rsp.remote_addr)}]:{rsp.remote_port}")
            print(f"Local Address:  [{ip_addr_bytes_to_str(rsp.local_addr)}]:{rsp.local_port}")
        print()

        if args.time:
            timeout_s = time.time() + args.time + 5
        else:
            timeout_s = time.time() + amount / MIN_EXPECTED_BW_BPS + 5
        interval_s = args.interval if args.interval else 2
        prev_duration = 0
        prev_bytes_transferred = 0
        while time.time() < timeout_s:
            rsp = dutif.exec("lwip/iperf_get_stats", port_type=port_type, dir="CLIENT")
            logging.debug(rsp)
            if rsp.state == ACE_ENUM.IperfState.FINISHED:
                print("---")
                print(f"Iperf summary: bytes transferred: {rsp.bytes_transferred}, duration: {rsp.ms_duration} ms, bandwidth: {rsp.bandwidth_kbitpsec} kbps")     # noqa: E501
                break
            if args.interval:
                interval_duration_ms = rsp.ms_duration - prev_duration
                interval_bytes_transferred = rsp.bytes_transferred - prev_bytes_transferred
                interval_bw_kbps = interval_bytes_transferred * 8 // interval_duration_ms
                prev_duration = rsp.ms_duration
                prev_bytes_transferred = rsp.bytes_transferred
                print(f"{rsp.ms_duration/1000:5.1f} s | {interval_bytes_transferred:9d} total bytes | {interval_bw_kbps:9d} kbps")     # noqa: E501
            time.sleep(interval_s)
    else:
        # --results flag set: retrieve results only
        rsp = dutif.exec("lwip/iperf_get_stats", port_type=port_type, dir="CLIENT")
        if rsp.state is None:
            rsp.state = 0
        if rsp.state in [ACE_ENUM.IperfState.FINISHED, ACE_ENUM.IperfState.RUNNING]:
            if len(rsp.remote_addr) == 4:
                print(f"Remote Address: {ip_addr_bytes_to_str(rsp.remote_addr)}:{rsp.remote_port}")         # noqa: E501
                print(f"Local Address:  {ip_addr_bytes_to_str(rsp.local_addr)}:{rsp.local_port}")           # noqa: E501
            else:
                print(f"Remote Address: [{ip_addr_bytes_to_str(rsp.remote_addr)}]:{rsp.remote_port}")       # noqa: E501
                print(f"Local Address:  [{ip_addr_bytes_to_str(rsp.local_addr)}]:{rsp.local_port}")         # noqa: E501
            if rsp.state == ACE_ENUM.IperfState.RUNNING:
                print("Iperf client running...")
                report_type = "Interim"
            else:
                print("Iperf client finished")
                report_type = "Final"

            print(f"{len(rsp.remote_addr)} {report_type} results: bytes transferred: {rsp.bytes_transferred}, duration: {rsp.ms_duration} ms, bandwidth: {rsp.bandwidth_kbitpsec} kbps")     # noqa: E501

        elif rsp.state == ACE_ENUM.IperfState.RUNNING:
            print("Iperf client is running")
        elif rsp.state == ACE_ENUM.IperfState.NOT_STARTED:
            print("Iperf client is not started")
        else:
            print(f"Iperf client in unknown state ({rsp.state})")


if __name__ == "__main__":
    _main()
