#!/usr/bin/env python3
#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

'''
Basic python script to send udp broadcast packets. Written to be used with the MM-IoT-SDK. This
script is designed to only require default python packages to run (there is an optional quality of
life package for coloured logs). This means that you can run this on any device that has python3
installed including the Morse Micro Linux based EVK APs.
'''

import argparse
import logging
import socket
import time


DEFAULT_PORT = 1337
DEFAULT_ADDR = "0.0.0.0"
DEFAULT_INTERVAL_SEC = 1
DEFAULT_DEVICE_COUNT = 1
ID = b'MMBC'


def _app_setup_args(parser):
    parser.add_argument('-p', '--port', default=DEFAULT_PORT, help='Port to bind the server to.')
    parser.add_argument('-a', '--addr', default=DEFAULT_ADDR, help='IP addr to bind the server to.')
    parser.add_argument('-d', '--device', help='Interface device to bind the server to.')
    parser.add_argument('-i', '--interval-sec', default=DEFAULT_INTERVAL_SEC, type=float,
                        help='Interval in seconds to send the udp broadcast packets.')
    parser.add_argument('-c', '--device-count', default=DEFAULT_DEVICE_COUNT, type=int,
                        help='Number of')


def color_generator(index, count, device_count):
    color_power = ((count // device_count) % 3) + 1

    if (count % device_count) >= index:
        rgb_value = 256**color_power - 1
    else:
        rgb_value = max(256**(color_power - 1) - 1, 0)

    return rgb_value.to_bytes(3, byteorder='big')


def construct_message(count, devices, color_gen):
    message = ID

    for i in range(devices):
        message += color_gen(i, count, devices)

    return bytearray(message)


def _app_main(args):
    print(f'Started UDP broadcast server.')
    print(f'Sending for {args.device_count} devices every {args.interval_sec} seconds.')

    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    server.bind((args.addr, args.port))

    if args.device is not None:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, args.device)

    # Enable port reusage so we will be able to run multiple clients and servers on single (host,
    # port).
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

    # Enable broadcasting mode
    server.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Set a timeout so the socket does not block
    # indefinitely when trying to receive data.
    server.settimeout(0.2)

    count = 0

    while True:
        message = construct_message(count, args.device_count, color_generator)
        server.sendto(message, ("255.255.255.255", args.port))
        logging.debug(f'Sent: {message}')

        count += 1
        try:
            time.sleep(args.interval_sec)
        except KeyboardInterrupt:
            break


#
# ---------------------------------------------------------------------------------------------
#

def _main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
                                     description=__doc__)
    parser.add_argument('-v', '--verbose', action='count', default=0,
                        help='Increase verbosity of log messages (repeat for increased verbosity)')
    parser.add_argument('-l', '--log-file',
                        help='Log to the given file as well as to the console')

    _app_setup_args(parser)

    args = parser.parse_args()

    # Configure logging
    log_handlers = [logging.StreamHandler()]
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))

    LOG_FORMAT = '%(asctime)s %(levelname)s: %(message)s'
    if args.verbose >= 2:
        logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT, handlers=log_handlers)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO, format=LOG_FORMAT, handlers=log_handlers)
    else:
        logging.basicConfig(level=logging.WARNING, format=LOG_FORMAT, handlers=log_handlers)

    # If coloredlogs package is installed, then use it to get pretty coloured log messages.
    # To install:
    #    pip3 install coloredlogs
    try:
        import coloredlogs
        coloredlogs.install(fmt=LOG_FORMAT, level=logging.root.level)
    except ImportError:
        logging.debug('coloredlogs not installed')

    _app_main(args)


if __name__ == '__main__':
    _main()
