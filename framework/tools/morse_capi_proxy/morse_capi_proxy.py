#!/usr/bin/env python3
#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

"""
Wi-Fi Alliance CAPI Control Agent Proxy for Morse Micro IoT SDK.

This tool should be used in conjuction with the Emmet example application.
"""

import argparse
import logging
import os
import socketserver
import sys

# Put ace directory into the Python path so that we can import  from acelib.
sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))) + "/ace")

from acelib import DutIf
from acelib.dutif import InvalidCommandError

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

DEFAULT_SERVER_HOSTNAME = "0.0.0.0"
DEFAULT_SERVER_PORT = 9999


class ParseError(Exception):
    pass


class CapiServerHandler(socketserver.BaseRequestHandler):
    def _send_line(self, line):
        self.request.send((line + "\r\n").encode("utf-8"))

    def _parse_arguments(self, arguments_tokens):
        args = {}
        param_name = None
        for token in arguments_tokens:
            if param_name is not None:
                args[param_name] = token
                param_name = None
            else:
                param_name = token.lower()
                if param_name in args:
                    raise ParseError(f"Multiple instances of {param_name}")
        if param_name is not None:
            raise ParseError(f"No value for parameter {param_name}")
        return args

    def handle_line(self, line):
        tokens = line.split(",")
        command_name = "capi/" + tokens[0].lower()

        try:
            args = self._parse_arguments(tokens[1:])
        except ParseError as e:
            logging.exception(e)
            self._send_line(f"status,INVALID,errorCode,{repr(e)}")
            return

        # Special case: ca_get_version
        if tokens[0] == "ca_get_version":
            self._send_line("status,RUNNING")
            self._send_line("status,COMPLETE,version,1.0")
            return

        try:
            # Do a dry run to catch possible issues before sending status,RUNNING.
            self.server.dutif.exec(cmd=command_name, exec_dry_run=True, **args)
        except InvalidCommandError as e:
            logging.exception(e)
            self._send_line("status,INVALID,errorCode,Invalid command or parameter")
            return
        except Exception as e:
            logging.exception(e)
            self._send_line(f"status,INVALID,errorCode,{repr(e)}")
            return

        self.logger.info("Executing command %s with args: %s", command_name, str(args))
        self._send_line("status,RUNNING")
        try:
            rsp = self.server.dutif.exec(cmd=command_name, **args)
            retstr = "status,COMPLETE"
            for name, value in rsp.args.items():
                value = "0" if value is None else value
                retstr += f",{name},{value}"
            self._send_line(retstr)
        except Exception as e:
            # Note we may still pick up parse errors in firmware, so maybe should also be
            # able to generate INVALID here depending on the error code.
            logging.exception(e)
            self._send_line(f"status,ERROR,errorCode,{repr(e)}")
            return

    def handle(self):
        self.logger = logging.getLogger(f"{self.client_address[0]}:{self.client_address[1]}")
        self.logger.info("New connection from %s:%d",
                         self.client_address[0], self.client_address[1])
        buf = b""
        while True:
            rx = self.request.recv(4096)
            if not rx:
                break
            buf += rx
            lines = buf.split(b"\r\n")
            buf = lines[-1]
            for line in lines[:-1]:
                line = line.strip().decode("utf-8")
                self.handle_line(line)
        self.logger.info("Disconnected")


class CapiServer(socketserver.TCPServer):
    allow_reuse_address = True

    def __init__(self, hostname, port, dutif):
        super().__init__((hostname, port), CapiServerHandler)
        self.dutif = dutif
        logging.info("Listening for connection on port %d", port)


def _app_setup_args(parser):
    parser.add_argument("-H", "--debug-host", required=True,
                        help="Hostname of machine on which OpenOCD is running")
    parser.add_argument("-g", "--gdb-port", default=DEFAULT_GDB_PORT, type=int,
                        help="GDB port to use")
    parser.add_argument("-t", "--tcl-port", default=DEFAULT_TCL_PORT, type=int,
                        help="OpenOCD TCL port to use")
    parser.add_argument("-p", "--port", default=DEFAULT_SERVER_PORT, type=int,
                        help="Server port to listen on")
    parser.add_argument("-b", "--hostname", default=DEFAULT_SERVER_HOSTNAME,
                        help="Hostname to bind to")


def _app_main(args):
    dutif = DutIf(debug_host=args.debug_host, gdb_port=args.gdb_port, tcl_port=args.tcl_port)

    with CapiServer(args.hostname, args.port, dutif) as server:
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass

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

    _app_setup_args(parser)

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

    # If coloredlogs package is installed, then use it to get pretty coloured log messages.
    # To install:
    #    pip3 install coloredlogs
    try:
        import coloredlogs
        coloredlogs.install(fmt=LOG_FORMAT, level=logging.root.level)
    except ImportError:
        logging.debug("coloredlogs not installed")

    _app_main(args)


if __name__ == "__main__":
    _main()
