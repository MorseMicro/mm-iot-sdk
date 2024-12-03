#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import logging
import socket

LOG_LEVEL_EXTRA = 5     # Extra verbosity beyond debug


class OpenOcdTcl:
    TERMINATOR = b"\x1a"

    def __init__(self, hostname, port=6666):
        self.hostname = hostname
        self.port = port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.logger = logging.getLogger(f"oocd@{hostname}:{port}")

    def open(self):     # noqa: A003
        try:
            self.socket.connect((self.hostname, self.port))
        except OSError as e:
            logging.error(f"OpenOcd TCL connection error ({self.hostname}:{self.port}): {e}")
            raise e

    def close(self):
        self.socket.shutdown(socket.SHUT_RDWR)
        self.socket.close()
        self.socket = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, type, value, tb):
        self.close()

    def read_until_terminator(self):
        """
        Read from the socket until we receive a terminator character. This should be the last
        thing we receive due to the stop and wait nature of the protocol.
        """
        rx_buf = bytearray()
        while True:
            # This is not efficient
            x = self.socket.recv(1)
            rx_buf += x
            if x == self.TERMINATOR:
                return rx_buf

    def _execute(self, cmd):
        raw_cmd = cmd.encode("utf-8") + self.TERMINATOR
        self.socket.send(raw_cmd)

        raw_rsp = self.read_until_terminator()
        rsp = raw_rsp[:-1].decode("utf-8")

        self.logger.log(LOG_LEVEL_EXTRA, f">> {cmd} -> {rsp}")
        return rsp

    def halt_target(self):
        self._execute("halt")

    def reset_halt_target(self):
        self._execute("reset halt")

    def reset_run_target(self):
        self._execute("reset run")

    def read_word(self, address):
        rsp = self._execute(f"mdw 0x{address:08x}")
        tokens = rsp.split(":")
        if len(tokens) < 2:
            return None
        else:
            return int(tokens[1], 16)

    def write_word(self, address, value):
        self._execute(f"mww 0x{address:08x} 0x{value:08x}")

    def flash_write_word(self, address, value):
        rsp = self._execute(f"flash fillw 0x{address:08x} 0x{value:08x} 1")
        if not rsp:
            raise RuntimeError("Failed to receive write confirmation")

    def flash_write_double_word(self, address, value):
        if (value > 0xffffffffffffffff):
            logging.error(f"0x{value:016x} > double_word")

        rsp = self._execute(f"flash filld 0x{address:08x} 0x{value:016x} 1")
        if not rsp:
            raise RuntimeError("Failed to receive write confirmation")

    def flash_read_word(self, address):
        rsp = self._execute(f"flash mdw 0x{address:08x} 1")
        tokens = rsp.split(":")
        if len(tokens) < 2:
            return None
        else:
            return int(tokens[1], 16)

    def flash_read_bytes(self, address, count):
        data = bytearray()
        while count > 0:
            # Read in chunks of 32 bytes which is the length of 1 line of mdb dump.
            # This is a reasonable compromise between speed and reliability of read data.
            if count > 32:
                read_count = 32
            else:
                read_count = count
            rsp = self._execute(f"flash mdb 0x{address:08x} 0x{read_count:08x}")
            tokens = rsp.split(":")
            if len(tokens) > 1:
                items = tokens[1].split(" ")
                for i in range(1, len(items) - 1):
                    data.append(int(items[i], 16))
            count -= read_count
            address += read_count
        return bytes(data)

    def flash_read_short(self, address):
        rsp = self._execute(f"flash mdh 0x{address:08x} 1")
        tokens = rsp.split(":")
        if len(tokens) < 2:
            return None
        else:
            return int(tokens[1], 16)

    def flash_write_bank(self, num, filename, offset=0):
        self._execute(f"flash write_bank {num} {filename} 0x{offset:08x}")

    def flash_erase_sector(self, num, first, last):
        self._execute(f"flash erase_sector {num} {first} {last}")

    def flash_write_image(self, filename):
        self._execute(f"flash write_image {filename}")

    def target_current(self):
        return self._execute("target current")

    def read_mem(self, address, nelems, width=8):
        ARRAYVAR = "readoutput"
        self._execute(f"array unset {ARRAYVAR}")
        self._execute(f"mem2array {ARRAYVAR} {width} {address} {nelems}")
        rsp = self._execute(f"return ${ARRAYVAR}")
        rsp = [*(int(x, 0) for x in rsp.split(" "))]
        d = dict([tuple(rsp[i:i + 2]) for i in range(0, len(rsp), 2)])
        return bytes([d[k] for k in sorted(d.keys())])

    def write_mem(self, address, data, width=8):
        ARRAYVAR = "senddata"
        dataarray = " ".join([f"{offset} 0x{val:x}" for offset, val in enumerate(data)])

        self._execute(f"array unset {ARRAYVAR}")
        self._execute(f"array set {ARRAYVAR} {{ {dataarray} }}")
        self._execute(f"array2mem {ARRAYVAR} {width} 0x{address:08x} {len(data)}")
