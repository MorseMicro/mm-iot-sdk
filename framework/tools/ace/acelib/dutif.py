#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import logging
import shlex
import subprocess
import sys
from threading import Lock

from acelib.openocdtcl import OpenOcdTcl

try:
    from google.protobuf.descriptor import FieldDescriptor

    from acelib import emmet_api_pb2
except ModuleNotFoundError as e:
    logging.error(e)
    logging.error("Try installing protobuf:")
    logging.error("    pip3 install protobuf")
    sys.exit(1)

from acelib.transport_mem import MemTransport

logger = logging.getLogger("DutIf")


class InvalidCommandError(Exception):
    pass


class InvalidResponseError(Exception):
    pass


class CommandTimeoutError(Exception):
    pass


class CommandExecutionError(Exception):
    def __init__(self, cmd, err_code, err_msg):
        msg = f"{cmd} failed"
        if err_code is not None:
            msg += f" with code {err_code} ({emmet_api_pb2.StatusCode.Name(err_code)})"
        if err_msg:
            msg += f": {err_msg}"
        super().__init__(msg)

        self.err_code = err_code
        self.err_msg = err_msg


class GdbError(Exception):
    def __init__(self, msg, completed_proc):
        super().__init__(msg)
        self.completed_proc = completed_proc


def parse_subcommands(cmdgrp_descriptor):
    commands = {}
    for field in cmdgrp_descriptor.message_type.fields:
        params = {}
        commands[field.name] = params
        for param_field in field.message_type.fields:
            param_name = param_field.name
            params[param_name] = param_field
    return commands


def parse_commands():
    commands = {}
    for field in emmet_api_pb2.DutCmd.DESCRIPTOR.fields:
        commands[field.name] = parse_subcommands(field)
    return commands


def print_commands():
    print("Supported commands")
    commands = parse_commands()
    for command_grp in sorted(commands):
        for command in sorted(commands[command_grp]):
            print(f"    {command_grp}/{command}")


class Response:
    def __init__(self, cmdgrp, subcmd, rsp_msg):
        self.args = {}
        if rsp_msg:
            if not rsp_msg.HasField(cmdgrp):
                logger.warn(f'Received invalid response (expect {cmdgrp}, '
                            f'got {rsp_msg.WhichOneof("rspgrp")})')
                raise InvalidResponseError
            rspgrp = getattr(rsp_msg, cmdgrp)

            if not rspgrp.HasField(subcmd):
                logger.warn(f'Received invalid response (expect {subcmd}, '
                            f'got {rspgrp.WhichOneof("rsp")})')
                raise InvalidResponseError

            field_descriptor = emmet_api_pb2.DutRsp.DESCRIPTOR.fields_by_name[cmdgrp]
            descriptor = field_descriptor.message_type.fields_by_name[subcmd]

            # Ensure we at least have an empty entry for each parameter
            for field in descriptor.message_type.fields:
                self.args[field.name] = None

            # Now fill in with actual received values
            for desc, value in getattr(rspgrp, subcmd).ListFields():
                self.args[desc.name] = value

    def __getattr__(self, name):
        if name in self.args:
            return self.args[name]
        else:
            raise AttributeError(f"Invalid attribute: {name}")

    def __str__(self):
        return f"Response{self.args}"


class DutIf:
    def __init__(self, debug_host, gdb_port, tcl_port,
                 gdb_command="arm-none-eabi-gdb",
                 gdb_command_timeout=60,
                 adapter_speed=None):
        self.debug_host = debug_host
        self.gdb_port = gdb_port
        self.tcl_port = tcl_port
        self.gdb_command = gdb_command
        self.gdb_command_timeout = gdb_command_timeout
        self.adapter_speed = adapter_speed
        self.transport = MemTransport(debug_host, tcl_port)

        self.cmd_lock = Lock()
        self.commands = parse_commands()

    def exec(self, cmd, exec_dry_run=False, **kwargs):  # noqa: A003
        logger.debug(f"Exec: {cmd}: {kwargs}")

        try:
            cmdgrp, subcmd = cmd.split("/")
            cmdinfo = self.commands[cmdgrp][subcmd]
        except KeyError as e:
            raise InvalidCommandError from e
        except ValueError as e:
            raise InvalidCommandError from e

        dutcmd = emmet_api_pb2.DutCmd()
        # Select the command message
        cmdgrpmsg = getattr(dutcmd, cmdgrp)
        cmdgrpmsg.SetInParent()
        cmdmsg = getattr(cmdgrpmsg, subcmd)
        cmdmsg.SetInParent()

        for param, arg in kwargs.items():
            # We have support for type conversions for a subset of types
            try:
                paramdesc = cmdinfo[param]
            except KeyError as e:
                # At the moment we are using InvalidCommandError for invalid commands
                # and invalid parameters. We may want to consider having a separate
                # InvalidParameter error.
                raise InvalidCommandError from e

            def _fix_arg_type(_arg):
                if paramdesc.type in [
                        FieldDescriptor.TYPE_INT32, FieldDescriptor.TYPE_INT64,
                        FieldDescriptor.TYPE_SINT32, FieldDescriptor.TYPE_SINT64,
                        FieldDescriptor.TYPE_UINT32, FieldDescriptor.TYPE_UINT64
                ]:
                    return int(_arg)

                if paramdesc.type == FieldDescriptor.TYPE_BOOL:
                    return bool(_arg)

                if paramdesc.type == FieldDescriptor.TYPE_ENUM:
                    return paramdesc.enum_type.values_by_name[_arg].number

                return _arg

            if paramdesc.label == FieldDescriptor.LABEL_REPEATED:
                arg = [_fix_arg_type(x) for x in arg]
                getattr(cmdmsg, param).extend(arg)
            else:
                setattr(cmdmsg, param, _fix_arg_type(arg))

        if exec_dry_run:
            return Response(cmdgrp, subcmd, None)

        with self.cmd_lock:
            rsp_data = self.transport.exec(dutcmd.SerializeToString())
            rsp = emmet_api_pb2.DutRsp()
            rsp.ParseFromString(rsp_data)
            if rsp.HasField("basic"):
                if rsp.basic.code == 0:
                    return Response(cmdgrp, subcmd, None)
                else:
                    raise CommandExecutionError(cmd, rsp.basic.code, rsp.basic.message)
            return Response(cmdgrp, subcmd, rsp)

    def _gdb_exec(self, commands, elf_file=None, confirm=False):
        cmdline = [self.gdb_command]
        if elf_file:
            cmdline.append(elf_file)
        if not confirm:
            cmdline += ["-ex", "set confirm off"]
        cmdline += ["-ex", f"target extended-remote {self.debug_host}:{self.gdb_port}"]
        for cmd in commands:
            cmdline += ["-ex", cmd]
        cmdline += ["-ex", "quit"]

        logger.debug(f'GDB Exec: {" ".join([shlex.quote(x) for x in cmdline])}')
        completed_proc = None
        try:
            completed_proc = subprocess.run(cmdline, capture_output=True,
                                            timeout=self.gdb_command_timeout, check=False)
        except subprocess.TimeoutExpired:
            logger.error("Timed out waiting for GDB to complete")
            raise GdbError("Timed out waiting for GDB to complete", completed_proc)

        if completed_proc.returncode != 0:
            logger.error(completed_proc.stderr.decode("utf-8"))
            raise GdbError(f'Failed to execute commands: {" ".join(commands)}', completed_proc)

        logger.debug("GDB stdout:")
        for line in completed_proc.stdout.splitlines(keepends=False):
            logger.debug(f"    {line}")
        logger.debug("GDB stderr:")
        for line in completed_proc.stderr.splitlines(keepends=False):
            logger.debug(f"    {line}")

    def load(self, elf_file):
        commands = [
            "monitor reset halt",
        ]
        if self.adapter_speed:
            commands.append(f"mon adapter speed {self.adapter_speed}")
        commands += [
            "load",
            "monitor reset halt"
        ]
        self._gdb_exec(commands=commands, elf_file=elf_file)

    def read_mem(self, output_filename, start_address, end_address):
        self._gdb_exec(
            commands=[
                f"dump binary mem {output_filename} 0x{start_address:08x} 0x{end_address:08x}"
            ],
            elf_file=None)

    def reset(self):
        with OpenOcdTcl(self.debug_host, self.tcl_port) as tclif:
            tclif.reset_run_target()

    def reset_halt(self):
        with OpenOcdTcl(self.debug_host, self.tcl_port) as tclif:
            tclif.reset_halt_target()

    def get_memalloc_log(self, elf_file, log_path):
        """
        Processor must be preliminarily halted.
        log_path: absolute path to the memory alloc ouput file
        elf_file: absolute path to the elf file
        """
        commands = [
            f"set logging file {log_path}",
            "set logging overwrite on",
            "set logging on",
            "p mem_allocations",
            "set logging off"
        ]
        self._gdb_exec(commands=commands, elf_file=elf_file)
