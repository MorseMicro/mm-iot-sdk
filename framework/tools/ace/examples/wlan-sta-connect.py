#!/usr/bin/env python3
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace WLAN STA connect to AP example
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
from acelib import ACE_ENUM, DutIf, mac_addr_str_to_bytes  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666
DEFAULT_POLL_TIMEOUT = 60
DEFAULT_RAW_STA_PRIORITY = -1
DEFAULT_STA_TYPE = 2
DEFAULT_BGSCAN_SHORT_INTERVAL_S = 0
DEFAULT_BGSCAN_THRESHOLD_DBM = 0
DEFAULT_BGSCAN_LONG_INTERVAL_S = 0
DEFAULT_TWT_WAKE_INTERVAL_US = 300000000
DEFAULT_TWT_MIN_WAKE_DURATION_US = 65280
MMWLAN_DEFAULT_SCAN_INTERVAL_LIMIT_S = 512


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
                        help="SSID of AP to connect to")
    parser.add_argument("-b", "--bssid",
                        help="BSSID of AP to connect to")
    parser.add_argument("-p", "--passphrase",
                        help="Passphrase of AP to connect to (required for SAE)")
    parser.add_argument("-S", "--security-type", choices=["OWE", "SAE"],
                        help="Security type to use (OWE, SAE). Do not specify for no security.")
    parser.add_argument("-T", "--poll-timeout", default=DEFAULT_POLL_TIMEOUT, type=float,
                        help='Timeout in seconds after which we give up on polling the device to see if it has connected')  # noqa
    parser.add_argument("--disable-pmf", action="store_true",
                        help="Disable protected management frame support.")
    parser.add_argument("-r", "--raw-sta-priority", default=DEFAULT_RAW_STA_PRIORITY, type=int,
                        help='Sets Restricted Access Window (RAW) priority for STA. (0-7, -1 for RAW disabled)')  # noqa
    parser.add_argument("-a", "--sta-type", default=DEFAULT_STA_TYPE, type=int,
                        help="Sets S1G non-AP STA type. (1- Sensor STA, 2- Non-sensor STA)")
    parser.add_argument("-G", "--sae-owe-ec-groups", nargs="+",
                        help="""
        List of optional SAE group IDs separated by space or single OWE DH group ID.
        Supported values [19 20 21]. If not set, group 19 is preferred.
    """)
    parser.add_argument("-c", "--cac-enable", action="store_true", help="Enable CAC support")
    parser.add_argument('-o', '--bgscan-short-interval-s', default=DEFAULT_BGSCAN_SHORT_INTERVAL_S, type=int,               # noqa
                        help='Background scan short interval in seconds. Setting to 0 will disable background scanning.')   # noqa
    parser.add_argument('-e', '--bgscan-threshold-dbm', default=DEFAULT_BGSCAN_THRESHOLD_DBM, type=int,                     # noqa
                        help="Background scan signal threshold in dBm.)")
    parser.add_argument('-l', '--bgscan-long-interval-s', default=DEFAULT_BGSCAN_LONG_INTERVAL_S, type=int,                 # noqa
                        help='Background scan long interval in seconds. Setting to 0 will disable background scanning.')    # noqa
    parser.add_argument('-L', '--scan-interval-limit-s', default=MMWLAN_DEFAULT_SCAN_INTERVAL_LIMIT_S,                      # noqa
                        help='Scan interval limit in seconds.')                                                             # noqa
    parser.add_argument('-w', '--twt-enabled', action='store_true', help='Enable Target Wake Time (TWT)')                   # noqa
    parser.add_argument('-k', '--twt-wake-interval-us', default=DEFAULT_TWT_WAKE_INTERVAL_US, type=int,                     # noqa
                        help="TWT service period interval in micro seconds.")
    parser.add_argument('-d', '--twt-min-wake-duration-us', default=DEFAULT_TWT_MIN_WAKE_DURATION_US, type=int,             # noqa
                        help="Minimum TWT wake duration in micro seconds.")
    parser.add_argument("-u", "--twt-setup-command", choices=["REQUEST", "SUGGEST", "DEMAND"],
                        help="TWT setup command. Only REQUEST and SUGGEST is supported).")

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

    if args.twt_setup_command == "SUGGEST":
        TWT_setup_command = "TWT_" + args.twt_setup_command
    elif args.twt_setup_command == "DEMAND":
        TWT_setup_command = "TWT_" + args.twt_setup_command
    else:
        TWT_setup_command = "TWT_REQUEST"

    passphrase = b""
    if args.passphrase:
        passphrase = args.passphrase.encode("utf-8")

    bssid = mac_addr_str_to_bytes(args.bssid) if args.bssid else b""

    sae_owe_ec_groups = args.sae_owe_ec_groups if args.sae_owe_ec_groups is not None else []

    dutif.exec("wlan/sta_enable",
               ssid=args.ssid.encode("utf-8"), bssid=bssid, passphrase=passphrase,
               security=security, pmf_mode=pmf_mode, raw_sta_priority=args.raw_sta_priority,
               sta_type=args.sta_type, sae_owe_ec_groups=sae_owe_ec_groups,
               cac_enabled=args.cac_enable, bgscan_short_interval_s=args.bgscan_short_interval_s,
               bgscan_signal_threshold_dbm=args.bgscan_threshold_dbm,
               bgscan_long_interval_s=args.bgscan_long_interval_s,
               scan_interval_limit_s=args.scan_interval_limit_s,
               twt_enabled=args.twt_enabled, twt_wake_interval_us=args.twt_wake_interval_us,
               twt_min_wake_duration_us=args.twt_min_wake_duration_us,
               twt_setup_command=TWT_setup_command)

    print("Waiting for connection...")

    timeout = time.time() + args.poll_timeout
    while time.time() < timeout:
        rsp = dutif.exec("wlan/sta_get_state")
        if rsp.state == ACE_ENUM.StaState.STA_CONNECTED:
            break
        time.sleep(1)


if __name__ == "__main__":
    _main()
