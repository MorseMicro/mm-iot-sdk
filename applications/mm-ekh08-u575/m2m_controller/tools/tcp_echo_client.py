#!/usr/bin/env python3
#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

'''
Basic python script to connect to M2M tcp_echo_server and send it packets and verify the echoed
data. Written to be used with the MM-IoT-SDK. This script is designed to only require default
python packages to run. This means that you can run this on any device that has python3
installed including the Morse Micro Linux based EVK APs.
'''

import argparse
import logging
import socket
import time
import random
import os

DEFAULT_PORT = 5000
DEFAULT_ADDR = "192.168.1.2"
DEFAULT_STREAM_COUNT = 1
DEFAULT_PACKET_COUNT = 1000

PACKET_SIZE_MIN = 1
PACKET_SIZE_MAX = 1536


def _app_setup_args(parser):
    parser.add_argument('-p', '--port', default=DEFAULT_PORT, help='Port to connect to echo server on.')
    parser.add_argument('-a', '--addr', default=DEFAULT_ADDR, help='IP addr to connect to.')
    parser.add_argument('-s', '--stream-count', default=DEFAULT_STREAM_COUNT, type=int,
                        help='Number of parallel TCP streams to open.')
    parser.add_argument('-c', '--packet-count', default=DEFAULT_PACKET_COUNT, type=int,
                        help='Number of packets to send per stream.')

    class NumBytesAction(argparse.Action):
        def __call__(self, parser, namespace, values, option_string=None):
            if values < PACKET_SIZE_MIN or values > PACKET_SIZE_MAX:
                parser.error(
                    f"{option_string} value must be in range {PACKET_SIZE_MIN}-{PACKET_SIZE_MAX}")
            setattr(namespace, self.dest, values)

    parser.add_argument('-b', '--num-bytes', default=1024, type=int, action=NumBytesAction,
                        help=f'Numbers of bytes in a packet ({PACKET_SIZE_MIN}-{PACKET_SIZE_MAX}')


def tcp_client(host, port, num_packets, num_bytes):
    try:
        client_socket = socket.socket()
        client_socket.connect((host, port))
        client_socket.settimeout(5)
    except Exception as e:
        logging.error(f"Socket connect failed with error {e}!")

    count = 1
    error_count = 0
    while (count <= num_packets):
        buffer = bytearray(os.urandom(num_bytes))
        try:
            client_socket.sendall(buffer)
            echo_data = client_socket.recv(num_bytes)
            print(f"Received {len(echo_data)} from socket.")
            if (buffer != echo_data):
                logging.error("Received echo data did not match!")
                error_count += 1
            else:
                logging.info(f"Successfully sent and received packet {count}")
            count += 1

        except socket.timeout:
            logging.error(f"Timeout during send/recv at packet {count}")
            error_count += 1
            time.sleep(1)

    client_socket.shutdown(socket.SHUT_RDWR)
    client_socket.close()

    logging.info(f"Finished sending {num_packets} packets, Total {error_count} errors.")


def _app_main(args):
    print(f'Started TCP echo test.')
    print(f'Sending {args.packet_count} {args.num_bytes} byte packets to {args.addr}:{args.port}...')
    tcp_client(args.addr, args.port, args.packet_count, args.num_bytes)


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

    _app_main(args)


if __name__ == '__main__':
    _main()
