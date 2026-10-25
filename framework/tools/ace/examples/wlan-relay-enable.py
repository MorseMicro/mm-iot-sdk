#!/usr/bin/env python3
#
# Copyright 2026 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace WLAN relay enable example
"""

import argparse
import os
import sys

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf, mac_addr_str_to_bytes  # noqa: E402

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
    parser.add_argument("-r", "--role", choices=["root", "sta", "repeater"], required=True,
                        help="Role of this device in the relay network")
    parser.add_argument("-s", "--ssid", required=True,
                        help="SSID of the relay network")
    parser.add_argument("-p", "--passphrase",
                        help="Passphrase of the relay network (required for SAE)")
    parser.add_argument("-S", "--security-type", choices=["SAE"],
                        help="Security type to use (SAE). Do not specify for no security.")
    parser.add_argument("--disable-pmf", action="store_true",
                        help="Disable protected management frame support.")
    parser.add_argument("-f", "--forwarding-table-size", default=0, type=int,
                        help="Forwarding table size to configure for relay mode")
    parser.add_argument("--recovery-timeout-ms", default=0, type=int,
                        help="Time to wait for the relay network to recover before retrying. "
                             "0 uses the firmware default. Unused by the root.")
    parser.add_argument("--halow_channel", default=44,
                        help="S1G channel number to use for the root AP")
    parser.add_argument("--op_class", default=71,
                        help="Global operating class to use for the root AP")
    parser.add_argument("--ap_bssid",
                        help="BSSID to use for the root/relay AP. Derived from the MAC if not set.")
    parser.add_argument("--beacon_int", default=100, help="Beacon interval TUs to use for the AP")
    parser.add_argument("--dtim_period", default=1, help="DTIM period to use for the AP")
    parser.add_argument("--max_stas", default="4",
                        help="Maximum number of stations that can connect to the AP")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if args.security_type:
        security = args.security_type + "_SECURITY"
    else:
        security = "NO_SECURITY"

    if args.disable_pmf:
        pmf_mode = "PMF_DISABLED"
    else:
        pmf_mode = "PMF_REQUIRED"

    passphrase = b""
    if args.passphrase:
        passphrase = args.passphrase.encode("utf-8")

    ap_bssid = mac_addr_str_to_bytes(args.ap_bssid) if args.ap_bssid else b""

    relay_args = {}
    if args.role == "root":
        relay_args["root_ap"] = {
            "ssid": args.ssid.encode("utf-8"),
            "bssid": ap_bssid,
            "security": security,
            "passphrase": passphrase,
            "pmf_mode": pmf_mode,
            "op_class": args.op_class,
            "s1g_chan_num": args.halow_channel,
            "beacon_interval_tus": args.beacon_int,
            "dtim_period": args.dtim_period,
            "max_stas": args.max_stas,
        }
    else:
        relay_args["relay_sta"] = {
            "ssid": args.ssid.encode("utf-8"),
            "security": security,
            "passphrase": passphrase,
            "pmf_mode": pmf_mode,
            "raw_sta_priority": -1,
        }

    if args.role == "repeater":
        relay_args["relay_ap"] = {
            "bssid": ap_bssid,
            "beacon_interval_tus": args.beacon_int,
            "dtim_period": args.dtim_period,
            "max_stas": args.max_stas,
        }

    dutif.exec("wlan/relay_enable",
               forwarding_table_size=args.forwarding_table_size,
               recovery_timeout_ms=args.recovery_timeout_ms,
               **relay_args)

    print(f"Enabled relay mode as {args.role}")


if __name__ == "__main__":
    _main()
