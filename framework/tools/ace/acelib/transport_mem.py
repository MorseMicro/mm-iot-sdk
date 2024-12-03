#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import logging
import time

from acelib.openocdtcl import OpenOcdTcl

logger = logging.getLogger("MemTransport")


class UnsupportedHostError(Exception):
    pass


class DeviceNotReadyError(Exception):
    pass


class CommandTooLongError(Exception):
    pass


class CommandTimeoutError(Exception):
    pass


"""
/*
 * States:
 *
 * | # | Name             | Command len   | Response len
 * |---|------------------|---------------|--------------
 * | 1 | Idle/ready       | 0             | 0
 * | 2 | Processing       | non-0         | 0
 * | 3 | Response ready   | non-0         | non-0
 * | 4 | Response acked   | 0             | non-0
 *
 *
 *
 * State 1:
 *   HOST                                   MCU
 *   copies command into buffer             polls for command len non-zero
 *   sets command len
 *
 * State 2:
 *   HOST                                   MCU
 *   polls for response len non-zero        xcvr retrieves command from buffer
 *                                          xcvr processes command
 *                                          once complete copies rsponse into response buffer
 *                                          sets response len
 *
 * State 3:
 *   HOST                                   MCU
 *   processes response                     polls for zero command len
 *   sets command len to zero
 *
 * State 4:
 *   HOST                                   MCU
 *   polls for response len zero            set response len to zero
 */
 """

HOSTIF_TABLE_OFFSETS = {
    "magic_number": 0x00000000,
    "command_buffer_address": 0x00000004,
    "command_buffer_length": 0x00000008,
    "command_length": 0x0000000C,
    "response_buffer_address": 0x00000010,
    "response_length": 0x00000014,
}

MAGIC_NUMBER = 0xa5defa3d

# Maximum amount of time to wait for the device to become ready
# after command completion
READY_TIMEOUT = 30


class MemTransport:
    # TODO: define table address
    def __init__(self, hostname, port, hostif_table_addr=0x20000400):
        self.hostname = hostname
        self.port = port
        self.hostif_table_addr = hostif_table_addr

    def read_table_reg(self, reg_name, tclif):
        addr = self.hostif_table_addr + HOSTIF_TABLE_OFFSETS[reg_name]
        return tclif.read_word(addr)

    def write_table_reg(self, reg_name, value, tclif):
        addr = self.hostif_table_addr + HOSTIF_TABLE_OFFSETS[reg_name]
        tclif.write_word(addr, value)

    def __send_cmd(self, cmd, tclif):
        # Copy command into buffer and set command_length
        max_command_len = self.read_table_reg("command_buffer_length", tclif)
        if len(cmd) > max_command_len:
            raise CommandTooLongError
        command_buf_addr = self.read_table_reg("command_buffer_address", tclif)
        tclif.write_mem(command_buf_addr, cmd)
        self.write_table_reg("command_length", len(cmd), tclif)

    def __poll_for_response(self, tclif, timeout=10):
        # Must be called after __send_cmd()
        timeout_time = time.time() + timeout
        while True:
            response_length = self.read_table_reg("response_length", tclif)
            if response_length != 0:
                break
            if time.time() > timeout_time:
                raise CommandTimeoutError
            time.sleep(0.01)

        response_buffer_address = self.read_table_reg("response_buffer_address", tclif)
        return tclif.read_mem(response_buffer_address, response_length)

    def __ack_response(self, tclif):
        # Acknowledge response by clearing command
        # Must be called after __poll_for_response()
        self.write_table_reg("command_length", 0, tclif)

    def __await_ready_state(self, tclif):
        # Wait for device to return response length to zero
        # Must be called after __ack_response()
        timeout_time = time.time() + READY_TIMEOUT
        while True:
            response_length = self.read_table_reg("response_length", tclif)
            if response_length == 0:
                break
            if time.time() > timeout_time:
                raise DeviceNotReadyError
            time.sleep(0.01)

    def __prepare_device(self, tclif, timeout=10):
        cmd_len = self.read_table_reg("command_length", tclif)
        rsp_len = self.read_table_reg("response_length", tclif)

        if (cmd_len != 0) and (rsp_len != 0):
            logger.info("Clearing hanging Command")
            self.__ack_response(tclif)
            self.__await_ready_state(tclif)
        elif cmd_len != 0 and rsp_len == 0:
            try:
                logger.info("Existing Command, await response")
                self.__poll_for_response(tclif, timeout)
            except CommandTimeoutError as e:
                logger.error("Existing Command with no response")
                raise DeviceNotReadyError from e

            logger.info("Response received, clearing")
            self.__ack_response(tclif)
            self.__await_ready_state(tclif)
        elif cmd_len != 0 or rsp_len != 0:
            raise DeviceNotReadyError

    def wait_for_transport_ready(self, tclif, timeout_s=5, poll_interval_s=0.5):
        timeout_time = time.time() + timeout_s
        while True:
            # Validate magic number
            response = self.read_table_reg("magic_number", tclif)
            if response == MAGIC_NUMBER:
                confirmation_time_s = timeout_s - (timeout_time - time.time())
                logger.debug(f"Confirmed transport ready within {confirmation_time_s} "
                             f"sec (p:{poll_interval_s})")
                break
            if time.time() > timeout_time:
                logger.error(f"Unable to confirm transport ready within {timeout_s} sec")
                raise UnsupportedHostError
            time.sleep(poll_interval_s)

        return confirmation_time_s

    def exec(self, cmd, timeout=120):  # noqa: A003
        with OpenOcdTcl(self.hostname, self.port) as tclif:

            self.wait_for_transport_ready(tclif)

            # Make sure device is ready for a command. If not throw an error
            self.__prepare_device(tclif)

            self.__send_cmd(cmd, tclif)

            response = self.__poll_for_response(tclif, timeout)

            self.__ack_response(tclif)

            self.__await_ready_state(tclif)
        return response
