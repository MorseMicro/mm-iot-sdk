#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace ping start example

The default behaviour is to start the ping module and then print the results every --interval.
This behaviour can be modified with the --start, --results and --kill flags.
"""

import argparse
import logging
import os
import sys
import time
from datetime import datetime, timedelta

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, ip_addr_bytes_to_str, ip_addr_to_bytes  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

DEFAULT_MAX_TIME = 0            # seconds
DEFAULT_TIME_INTERVAL = 1000    # 1000 milliseconds
DEFAULT_PING_COUNT = 0
DEFAULT_DATA_SIZE = 56          # bytes

STATS_POLL_PERIOD = 0.5         # seconds
STATS_POLL_ITERATIONS = 30


def get_ping_stats(dutif):
    # Get ping stats
    rsp = dutif.exec("lwip/ping_stats")
    rsp.num_sent = 0 if rsp.num_sent is None else rsp.num_sent
    rsp.num_received = 0 if rsp.num_received is None else rsp.num_received
    rsp.min_latency = 0 if rsp.min_latency is None else rsp.min_latency
    rsp.avg_latency = 0 if rsp.avg_latency is None else rsp.avg_latency
    rsp.max_latency = 0 if rsp.max_latency is None else rsp.max_latency
    rsp.max_latency = 0 if rsp.max_latency is None else rsp.max_latency
    return rsp


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
    parser.add_argument("-I", "--ip-address", required=True,
                        help="IP address of the device to ping")
    parser.add_argument("-w", "--max-time", default=DEFAULT_MAX_TIME, type=int,
                        help="Max time for ping session to run for (in seconds)")
    parser.add_argument("-c", "--max-count", default=DEFAULT_PING_COUNT, type=int,
                        help="Max count for ping request")
    parser.add_argument("-i", "--interval", default=DEFAULT_TIME_INTERVAL, type=int,
                        help="Time interval for ping request")
    parser.add_argument("-s", "--data-size", default=DEFAULT_DATA_SIZE, type=int,
                        help="Data Size for ping request")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-S", "--start", action="store_true",
                       help="""
        If specified, the script will terminate immediately after starting the ping requests
    """)
    group.add_argument("-R", "--results", action="store_true",
                       help="""
        If specified, then the script will not start ping, it will just print the latest results
    """)
    group.add_argument("-K", "--kill", action="store_true",
                       help="""
        If specified, then the script will stop ping, it will also print the lastest results
    """)
    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    ip_address = ip_addr_to_bytes(args.ip_address)

    if not args.results:
        if not args.kill:
            # Start ping request
            print("Ping started...")
            rsp = dutif.exec("lwip/ping_request",
                             ip_address=ip_address,
                             max_count=args.max_count,
                             interval=args.interval,
                             data_size=args.data_size)

            # If the start flag was provided then we do not want to wait for results
            if args.start:
                return

            # This timeout is used to stop ping request. After this timeout, a ping_stop
            # is send to stop the mmping task and a ping_stats is send to get ping statistics.
            timeout = None
            if args.max_time:
                timeout = datetime.now() + timedelta(seconds=args.max_time)
                print(datetime.now())
                print(timeout)
            ping_running = True
            aborted_by_user = False
            prev_num_sent = 0
            while ping_running:
                try:
                    interval_s = args.interval / 1000
                    time.sleep(interval_s / 2)
                except KeyboardInterrupt:
                    aborted_by_user = True
                    break
                if timeout is not None and timeout <= datetime.now():
                    logging.info("Session timed out")
                    dutif.exec("lwip/ping_stop")
                rsp = get_ping_stats(dutif)
                if rsp.num_sent != prev_num_sent:
                    print(f"packets transmitted/received = {rsp.num_sent}/{rsp.num_received}, round-trip min/avg/max = {rsp.min_latency}/{rsp.avg_latency}/{rsp.max_latency} ms")       # noqa: E501
                    prev_num_sent = rsp.num_sent
                ping_running = rsp.active

            # If the ping was aborted by CTRL-C then stop it
            if aborted_by_user:
                dutif.exec("lwip/ping_stop")
        else:
            print("Stopping Ping request...")
            dutif.exec("lwip/ping_stop")

        # Get ping stats
        rsp = get_ping_stats(dutif)
        if (rsp.num_sent == 0):
            loss = 0
        else:
            loss = ((rsp.num_sent - rsp.num_received) * 100 / rsp.num_sent)
        print(f"\n--- {ip_addr_bytes_to_str(rsp.ip_address)} ping statistics ---\n{rsp.num_sent} packets transmitted, {rsp.num_received} packets received, {loss}% packet loss")   # noqa: E501
        print(f"round-trip min/avg/max = {rsp.min_latency}/{rsp.avg_latency}/{rsp.max_latency} ms\n")     # noqa: E501
        if rsp.active:
            print("Ping still running...\n")
    else:
        print("Getting ping stats...")
        rsp = get_ping_stats(dutif)
        if (rsp.num_sent == 0):
            loss = 0
        else:
            loss = ((rsp.num_sent - rsp.num_received) * 100 / rsp.num_sent)
        print(f"\n--- {ip_addr_bytes_to_str(rsp.ip_address)} ping statistics ---\n{rsp.num_sent} packets transmitted, {rsp.num_received} packets received, {loss}% packet loss")   # noqa: E501
        print(f"round-trip min/avg/max = {rsp.min_latency}/{rsp.avg_latency}/{rsp.max_latency} ms\n")     # noqa: E501
        if rsp.active:
            print("Ping still running...\n")


if __name__ == "__main__":
    _main()
