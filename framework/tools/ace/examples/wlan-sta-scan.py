#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace WLAN STA Scan example
"""

import argparse
import os
import sys
import time

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, mac_addr_bytes_to_str  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666
DEFAULT_DWELL_TIME_MS = 105
DEFAULT_POLL_TIMEOUT_SEC = 30


def _print_results(rsp, extended=False):
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
    parser.add_argument("-x", "--extended-results", action="store_true",
                        help="Print extended probe response results")

    parser.add_argument("-d", "--dwell-time-ms", default=DEFAULT_DWELL_TIME_MS, type=int,
                        help="""
        Time in milliseconds to dwell on a channel waiting for probe responses/beacons.
    """)
    parser.add_argument("-e", "--extra-ies",
                        help="""
        Optional Extra Information Elements to include in Probe Request frames.
    """)
    parser.add_argument("-s", "--ssid",
                        help="""
        Optional SSID in Probe Request frames for directed scan.
    """)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-S", "--start", action="store_true",
                       help="""
        If specified, the script will terminate immediately after starting the scan request.
    """)
    group.add_argument("-A", "--abort", action="store_true",
                       help="""
        If specified, the script will abort any pending or in progress scans.
    """)
    group.add_argument("-R", "--result", action="store_true",
                       help="""
        If specified, the script will print the latest scan results if available.
    """)

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    extra_ies = bytes.fromhex(args.extra_ies) if args.extra_ies else b""

    ssid = args.ssid.encode("utf-8") if args.ssid else b""

    if args.abort:
        dutif.exec("wlan/sta_scan_abort")
        return

    if args.result:
        rsp = dutif.exec("wlan/sta_scan_results")
        if rsp.scan_results is None:
            print("Scanning in progress...")
        else:
            _print_results(rsp, args.extended_results)
        return

    dutif.exec("wlan/sta_scan_request",
               dwell_time_ms=args.dwell_time_ms, extra_ies=extra_ies, ssid=ssid)

    if not args.start:
        print("Waiting for scan results...")
        timeout = time.time() + DEFAULT_POLL_TIMEOUT_SEC
        while time.time() < timeout:
            rsp = dutif.exec("wlan/sta_scan_results")
            if rsp.scan_results is not None:
                _print_results(rsp, args.extended_results)
                break
            time.sleep(1)


if __name__ == "__main__":
    _main()
