#!/usr/bin/env python3
#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

"""
Emmet/Ace Basic script used for genrating user input for WFA tests such as HAL 5.1.2
"""

import argparse
import logging
import os
import sys
import time

# Put ace directory into the Python path so that we can import  from acelib.
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))) + "/ace")
from acelib import DutIf, mac_addr_bytes_to_str  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666
DEFAULT_DWELL_TIME_MS = 105
DEFAULT_POLL_TIMEOUT_SEC = 30


def _print_scan_results(rsp, extended=False):
    headings = ["No#", "RSSI", "BSSID", "SSID", "Beacon Interval", "Capabilities"]
    if extended:
        headings.append("Information Elements")

    results_table = [headings]

    # Formate scan result fields
    for i, entry in enumerate(rsp.scan_results):
        result = []
        result.append(f"{i}")
        result.append(f"{entry.rssi:3d}")
        result.append(mac_addr_bytes_to_str(entry.bssid))
        result.append(f'{entry.ssid.decode("utf-8")}')
        result.append(f"{entry.beacon_interval}")
        result.append("".join([f"0x{entry.capability_info:04x}"]))

        if extended:
            result.append("0x" + "".join([f"{octet:02x}" for octet in entry.ies]))

        results_table.append(result)

    # Calculate column widths
    col_widths = [max(len(str(item)) for item in col) for col in zip(*results_table)]

    # Print table
    for row in results_table:
        for item, width in zip(row, col_widths):
            print(f"{str(item):<{width + 2}}", end="")
        print()

    print("Scan completed.")

def _handle_scan(args, dutif):
    dutif.exec("wlan/sta_scan_request", dwell_time_ms=args.dwell_time_ms)

    print("Waiting for scan results...")
    timeout = time.time() + DEFAULT_POLL_TIMEOUT_SEC
    while time.time() < timeout:
        rsp = dutif.exec("wlan/sta_scan_results")
        if rsp.scan_results is not None:
            _print_scan_results(rsp )
            break
        time.sleep(1)

def _handle_associate(args, dutif):
    dutif.exec("wlan/sta_enable",
               ssid=args.ssid.encode("utf-8"),
               passphrase=args.passphrase.encode("utf-8"),
               security="SAE_SECURITY")
    print("Sending association command...")
    time.sleep(2)
    print("Association triggered...")

#
# ---------------------------------------------------------------------------------------------
#

def _main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
                                     description=__doc__)
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="Increase verbosity of log messages (repeat for increased verbosity)")
    parser.add_argument("-l", "--log-file",
                        help="Log to the given file as well as to the console")

    parser.add_argument("-H", "--debug-host", default="localhost",
                        help="Hostname of machine on which OpenOCD is running")
    parser.add_argument("-g", "--gdb-port", default=DEFAULT_GDB_PORT, type=int,
                        help="GDB port to use")
    parser.add_argument("-t", "--tcl-port", default=DEFAULT_TCL_PORT, type=int,
                        help="OpenOCD TCL port to use")

    parser.add_argument("-d", "--dwell-time-ms", default=DEFAULT_DWELL_TIME_MS, type=int,
                        help="""
        Time in milliseconds to dwell on a channel waiting for probe responses/beacons.
    """)
    parser.add_argument("-s", "--ssid", default="MorseMicro", help="SSID of AP to connect to")
    parser.add_argument("-p", "--passphrase", default="12345678",
                        help="Passphrase of AP to connect to")

    args = parser.parse_args()

    # Configure logging
    log_handlers = [logging.StreamHandler()]
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))

    LOG_FORMAT = "%(asctime)s %(levelname)s %(name)s: %(message)s"
    if args.verbose >= 2:
        logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT, handlers=log_handlers)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO, format=LOG_FORMAT, handlers=log_handlers)
    else:
        logging.basicConfig(level=logging.WARNING, format=LOG_FORMAT, handlers=log_handlers)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)
    while True:
        print("\nPlease choose one of the following options:")
        print("S: Scan")
        print("A: Associate")
        print("Q: Quit")

        user_input = input("Enter your choice (S/A/Q): ").strip().upper()

        if user_input == "S":
            _handle_scan(args, dutif)
        elif user_input == "A":
            _handle_associate(args, dutif)
        elif user_input == "Q":
            print("\nGoodbye!")
            break
        else:
            print("\nInvalid choice, please try again.")

if __name__ == "__main__":
    _main()
