#!/usr/bin/env python3
#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Emmet/Ace get WLAN rate control statistics
"""

import argparse
import curses
import os
import sys
import time

import morse_common

# Because this script is in the examples subdirectory we need to put the parent directory into
# the Python path so that Python can find acelib. (This is not necessary when acelib is already
# in the Python path.)
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from acelib import DutIf  # noqa: E402

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

HEADER = "BW MCS Flags          #success | #attempts"
TOO_MANY_ROWS = " <too many rows to display (hint: use -z to show only non-zero rows)>"


def _retrieve_stats(args, dutif):
    stats = []
    rsp = dutif.exec("wlan/get_rc_stats")
    for i, rate_info in enumerate(reversed(rsp.rate_info)):
        # Since we reversed the list we need to unreverse the index
        i = len(rsp.rate_info) - 1 - i

        # Extract fields from rate_info (see mmwlan.h for magic numbers)
        bw = 1 << (rate_info & 0x0f)
        mcs = (rate_info >> 4) & 0x0f
        gi = "SGI" if (rate_info & (1 << 8)) else "   "

        success = rsp.total_success[i]
        sent = rsp.total_sent[i]

        if not args.hide_zero or success != 0 or sent != 0:
            stats.append(f"{bw:2}  {mcs:2} {gi}          {success:>10}   {sent:<10}")
    return stats


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

    parser.add_argument("-f", "--follow", action="store_true",
                        help="Keep updating every second until interrupted")
    parser.add_argument("-z", "--hide-zero", action="store_true",
                        help="Hide lines with no attempts or success")

    args = parser.parse_args()

    morse_common.configure_logging(args.verbose)

    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    if not args.follow:
        stats = _retrieve_stats(args, dutif)
        print(HEADER)
        for line in stats:
            print(line)
    else:
        def _curses_main(stdscr):
            max_height, max_width = stdscr.getmaxyx()
            while True:
                line_num = 1
                stats = _retrieve_stats(args, dutif)
                stdscr.clear()
                stdscr.addstr(HEADER, curses.A_BOLD)
                stdscr.addch("\n")
                for line in stats:
                    line_num += 1
                    if line_num == max_height:
                        stdscr.addstr(TOO_MANY_ROWS)
                        break
                    stdscr.addstr(line)
                    stdscr.addch("\n")
                stdscr.refresh()
                try:
                    time.sleep(1)
                except KeyboardInterrupt:
                    break

        curses.wrapper(_curses_main)


if __name__ == "__main__":
    _main()
