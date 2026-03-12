#!/usr/bin/env python3
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace WLAN AP enable example
"""

import argparse
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

    parser.add_argument("-s", "--ssid", required=True,
                        help="SSID to configure the AP with")
    parser.add_argument("-p", "--passphrase",
                        help="Passphrase to configure the AP with (required for SAE)")
    parser.add_argument("-S", "--security-type", choices=["SAE"],
                        help="Security type to use (SAE). Do not specify for no security.")

    parser.add_argument("--disable-pmf", action="store_true",
                        help="Disable protected management frame support.")
    parser.add_argument("-G", "--sae-owe-ec-groups", nargs="+",
                        help="""
        List of optional SAE group IDs separated by space or single OWE DH group ID.
        Supported values [19 20 21]. If not set, group 19 is preferred.
    """)

    parser.add_argument("--halow_channel", default=44, help="S1G channel number to use for AP")
    parser.add_argument("--op_class", default=71, help="Global operating class to use for AP")
    parser.add_argument("--halow_primary_bandwidth", default=2, help="Primary BW to use on AP")
    parser.add_argument("--halow_primary_channel_index", default="auto",
                        help="Primary channel index to use on AP")
    parser.add_argument("--beacon_int", default=100, help="Beacon interval TUs to use for AP")
    parser.add_argument("--dtim_period", default=1, help="DTIM period to use for AP")
    parser.add_argument("--halow_ip", default="192.168.1.1",
                        help="IP address to use on halow interface of AP")
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

    sae_owe_ec_groups = args.sae_owe_ec_groups if args.sae_owe_ec_groups is not None else []

    if args.halow_primary_channel_index == "auto":
        pri_1mhz_chan_idx = 0
    else:
        pri_1mhz_chan_idx = int(args.halow_primary_channel_index)

    dutif.exec("wlan/ap_enable",
               ssid=str(args.ssid).encode("utf-8"),
               security=security,
               passphrase=passphrase,
               pmf_mode=pmf_mode,
               sae_owe_ec_groups=sae_owe_ec_groups,
               op_class=args.op_class,
               s1g_chan_num=args.halow_channel,
               beacon_interval_tus=args.beacon_int,
               dtim_period=args.dtim_period,
               pri_bw_mhz=args.halow_primary_bandwidth,
               pri_1mhz_chan_idx=pri_1mhz_chan_idx,
               max_stas=args.max_stas)


if __name__ == "__main__":
    _main()
