#!/usr/bin/env python3
#
# Copyright 2022-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

"""
Basic script to program the configuration store into flash for supported MM-IOT-SDK platforms.

For more information about the configuration store see: src/mmconfig/mmconfig.c.

This script uses OpenOCD and GDB to program the flash. OpenOCD must be started before invoking
this script. The hostname, TCL port, and GDB port of the OpenOCD server may be specified using
the -H, -t, and -g parameters respectively. The GCC toolchain (specifically objcopy and gdb)
will need to be in your path.
"""

import argparse
import logging
import os
import re
import struct
import subprocess
import sys
import tempfile
import traceback

import hjson

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

sys.path.append(f"{SCRIPT_DIR}/../ace")
import acelib
from acelib.openocdtcl import OpenOcdTcl

DEFAULT_TOOLCHAIN_PREFIX = "arm-none-eabi-"

DEFAULT_GDB_PORT = 3333
DEFAULT_TCL_PORT = 6666

# MMCS (Morse Micro Config Store) in little endian notation
MMCONFIG_SIGNATURE = 0x53434D4D

# Starting seed value for the Xorshift PRNG, aribtrarily chosen
XORHASH_SEED = 0xdfb7f3e1

# Flash erased value
ERASED_VALUE = 0xFF

# Flash erased value
MAX_KEY_LEN = 32

# Regular expresion used to validate config store keys
KEY_PATTERN = re.compile("[A-Za-z][A-Za-z0-9._]*")

# Number of times to attempt a read before giving up
MAX_READ_ATTEMPTS = 2


class XorHash:
    def __init__(self):
        self._checksum = XORHASH_SEED

    def update(self, data):
        for b in data:
            self._checksum = XorHash._xorhash_value(self._checksum + b)

    def update_u8(self, u8):
        self._checksum = XorHash._xorhash_value(self._checksum + u8)

    def update_u16(self, u16):
        self.update(struct.pack("<H", u16))

    def checksum(self):
        return self._checksum

    def _xorhash_value(val):
        # Implements a simple hashing algorithm based on the Xorshift PRNG
        # https://en.wikipedia.org/wiki/Xorshift (Marsaglia, 2003)
        val = (val ^ (val << 13)) & 0xFFFFFFFF
        val = (val ^ (val >> 17)) & 0xFFFFFFFF
        val = (val ^ (val << 5)) & 0xFFFFFFFF
        return val


class ConfigStoreDeserializeError(Exception):
    pass


class ConfigStoreReadError(Exception):
    pass


class ProgrammingError(Exception):
    pass


class ConfigStorePartition:
    def __init__(self, version=0):
        self._dictionary = {}
        self._version = version

    def _serialize_key_value_pair(key, data):
        """
        Serialize the key/value pair in the following format:

        .. code-block:: text

            +------------+----------+--------------+-------------------+
            | Key Length | Key Name | Value Length | Value (raw bytes) |
            +------------+----------+--------------+-------------------+
                  1      <Key Length>       2          <Value Length>     (field size in octets)
        """
        result = struct.pack("<B", len(key)) + key.encode("ascii")
        result += struct.pack("<H", len(data)) + data
        return result

    def serialize(self, ostream):
        """
        Serialize the given ConfigStoreState data structure to the given file.
        """
        data = b""
        for k, v in self._dictionary.items():
            data += ConfigStorePartition._serialize_key_value_pair(k, v)

        # Compute checksum
        xorhash = XorHash()
        xorhash.update(data)

        ostream.write(struct.pack("<I", MMCONFIG_SIGNATURE))
        ostream.write(struct.pack("<I", self._version))
        ostream.write(struct.pack("<I", xorhash.checksum()))
        ostream.write(data)

        # Write a terminating 0xFF for Flashes that erase to non 0xFF values
        ostream.write((ERASED_VALUE).to_bytes(1, byteorder="little"))

    def deserialize(istream):
        verify_signature = istream.read_u32()
        version = istream.read_u32()
        checksum = istream.read_u32()

        xorhash = XorHash()

        partition = ConfigStorePartition(version=version)

        # Bail out if signature does not match
        if verify_signature != MMCONFIG_SIGNATURE:
            raise ConfigStoreDeserializeError("Invalid signature found")
            return None

        while not istream.is_at_eof():
            key_size = istream.read_u8()
            if key_size in [0, ERASED_VALUE]:
                # We reached the end of the list
                break

            # Sanity checks
            if key_size > MAX_KEY_LEN:
                raise ConfigStoreDeserializeError(
                    "Invalid key length in partition")

            xorhash.update_u8(key_size)

            raw_key = istream.read(key_size)
            xorhash.update(raw_key)

            value_size = istream.read_u16()
            if (value_size >> 8) == ERASED_VALUE:
                # Flash partially programmed, bomb out
                raise ConfigStoreDeserializeError("Corrupted partition detected")

            xorhash.update_u16(value_size)

            value = istream.read(value_size)
            xorhash.update(value)

            # Sanity checks
            try:
                key = raw_key.decode("ascii")
            except UnicodeDecodeError as e:
                raise ConfigStoreDeserializeError(f"Key contains non-ascii characters ({e})")

            if not KEY_PATTERN.fullmatch(key):
                raise ConfigStoreDeserializeError(f"Invalid characters found in key {key}")

            if len(key) > MAX_KEY_LEN:
                raise ConfigStoreDeserializeError(f"Key too long ({key})")

            partition._dictionary[key.lower()] = value
            logging.debug(f"Read {key}={value}")

        # Bail out if checksum does not match
        if xorhash.checksum() != checksum:
            raise ConfigStoreDeserializeError("Checksum failed")

        logging.debug("Successfully read partition with version %d", partition._version)
        return partition

    def copy_and_bump_version(self):
        partition = ConfigStorePartition(self._version + 1)
        partition._dictionary.update(self._dictionary)
        return partition

    def version(self):
        return self._version

    def _add_entry(self, type_name, data):
        if KEY_PATTERN.fullmatch(type_name) and len(type_name) <= MAX_KEY_LEN:
            # We do a .lower() as Python dictionaries are case sensitive but config store
            # is case insensitive and so would consider 'Key' and 'key' the same.
            self._dictionary[type_name.lower()] = data
        else:
            logging.error("Invalid key specified: %s", type_name)
            sys.exit(1)

    def add_entry_string(self, key, value):
        # It is valid for a value to be None in which case it means delete
        if value is not None:
            value = str(value)
            # MMCONFIG requires NULL terminator to be explicitly added to string data
            self._add_entry(key, value.encode("utf-8") + b"\0")
            logging.debug(f"Added string {key}")
        else:
            # Delete the entry from the configstore if it exists (no-op if it does
            # not exist).
            self.delete_entry(key, ignore_no_exist=True)
            logging.debug(f"Deleted entry for string {key}")

    def add_entry_hex_string(self, key, hex_string):
        try:
            value = bytes.fromhex(hex_string)
        except (ValueError, TypeError):
            logging.error(f"Invalid hex-string value given for key: {key}. "
                          'Hex strings specified in JSON should be surrounded by quotes (").')
            raise

        self._add_entry(key, value)

        logging.debug(f"Added bytes {key}")

    def add_entry_file(self, key, file):
        with open(file, "rb") as f:
            self._add_entry(key, f.read())

        logging.debug(f"Added file {file}")

    def delete_entry(self, type_name, ignore_no_exist=False):
        if type_name.lower() in self._dictionary:
            del self._dictionary[type_name.lower()]
        elif not ignore_no_exist:
            logging.error("Key not found")
            sys.exit(1)

    def read_entry(self, type_name):
        if type_name.lower() in self._dictionary:
            return self._dictionary[type_name.lower()]
        else:
            logging.error("Key not found")
            sys.exit(1)

    def match(self, other):
        """
        Compares the contents (not version numbers) of the two partitions and returns True
        if they match, else False.
        """
        if other is None:
            return False
        return self._dictionary == other._dictionary

    def dump(self):
        print(f"  Version {self._version}")
        print()
        print(f'  {"Key":{MAX_KEY_LEN}s} | Value')
        print(f'  {"-"*(MAX_KEY_LEN+1)}|--------')
        for key, value in self._dictionary.items():
            try:
                value = value.decode("utf-8")
            except UnicodeDecodeError:
                value = "[hex] " + value.hex()
                if len(value) > 55:
                    value = value[:55] + " ..."
            print(f"  {key:{MAX_KEY_LEN}s} | {value}")


class ConfigStore:
    def __init__(self, partitions):
        self._partitions = partitions
        self._update()

    def update_partition(self, partition_num, partition):
        self._partitions[partition_num] = partition
        self._update()

    def _update(self):
        if self._partitions[0] is not None:
            if self._partitions[1] is not None:
                # Both partitions valid, check which is newer
                if self._partitions[0].version() > self._partitions[1].version():
                    # Partition 0 is newer
                    self._newest_partition_num = 0
                else:
                    # Partition 1 is newer
                    self._newest_partition_num = 1
                logging.debug("Both partitions valid. Newest: %d; Versions: %d, %d",
                              self._newest_partition_num,
                              self._partitions[0].version(),
                              self._partitions[1].version())
            else:
                # Only partition 0 is valid
                self._newest_partition_num = 0
        else:
            if self._partitions[1] is not None:
                # Only partition 1 is valid
                self._newest_partition_num = 1
            else:
                # Neither is valid
                self._newest_partition_num = None
        if self._newest_partition_num is not None:
            self.staging = self._partitions[self._newest_partition_num].copy_and_bump_version()
        else:
            self.staging = ConfigStorePartition(version=1)

    def oldest_partition_num(self):
        if self._newest_partition_num is None:
            return 0
        else:
            return ((self._newest_partition_num + 1) % 2)

    def sanity_check(self):
        """
        Ensure that this data structure is internally consistent.
        """
        if self._newest_partition_num is not None:
            assert self._partitions[self._newest_partition_num] is not None
            newest_partition = self._partitions[self._newest_partition_num]
            assert self.staging.version() == newest_partition.version() + 1
        else:
            assert self.staging.version() == 1

    def has_staged_changes(self):
        if self._newest_partition_num is None:
            logging.debug("No existing partitions, so writing implicitly staged")
            return True
        return not self.staging.match(self._partitions[self._newest_partition_num])

    def dump(self):
        if self._partitions[0] is None and self._partitions[1] is None:
            print("Config store is empty")
            return

        for partition_num, desc in [
            (self._newest_partition_num, "current"),
            (self.oldest_partition_num(), "old")
        ]:
            if self._partitions[partition_num] is not None:
                print()
                header_str = f"Partition {partition_num} ({desc})"
                print(header_str)
                print("=" * len(header_str))
                print()
                self._partitions[partition_num].dump()
                print()


class ConfigStoreOpenOCDProgrammer:
    def __init__(self, toolchain_prefix, debug_host, gdb_port,
                 tcl_port, config_params, platform=None):

        self.toolchain_prefix = toolchain_prefix

        if platform is not None:
            target = self._find_target_for_platform(config_params, platform)
        else:
            logging.info("Attempting to auto-detect platform")
            target = self._auto_detect_target(config_params, debug_host, tcl_port)

        logging.info(f"Loading config for platform {target}")
        self._load_params(config_params, target)

        self._tclif = OpenOcdTcl(debug_host, tcl_port)
        self.dutif = acelib.dutif.DutIf(debug_host, gdb_port, tcl_port,
                                        gdb_command=f"{toolchain_prefix}gdb")
        self.cache = None

    def __enter__(self):
        self._tclif.open()
        return self

    def __exit__(self, type, value, tb):
        self._tclif.close()

    def _find_target_for_platform(self, config_params, platform):
        for target, params in config_params.items():
            if platform in params["supported_platforms"]:
                return target

        logging.error(f"Unable to find target config for platform: {platform}")
        raise ProgrammingError("Unable to find config for platform")

    def _auto_detect_target(self, config_params, debug_host, tcl_port):
        """
        Temporarily open a TCL socket so we can enquire which platform we are connected to. It does
        this by matching the current attached target in openocd with the platforms in the config
        file.
        """
        with OpenOcdTcl(debug_host, tcl_port) as tcl:
            target = tcl.target_current()
            if target not in config_params.keys():
                logging.error(f"Target {target} does not match any entry in the config file")
                raise ProgrammingError("Unable to find matching target")
            logging.info(f"Found matching entry for {target}.")
            logging.info(f'Supported platforms: {config_params[target]["supported_platforms"]}')

        return target

    def _load_params(self, config_params, target):
        # Convert configstore parameters that were read from JSON into integers, automatically
        # inferring the correct base
        for param in ["configstore_first_sector", "configstore_last_sector", "configstore_bank",
                      "configstore_base_address", "configstore_size"]:
            try:
                strvalue = str(config_params[target][param])
                setattr(self, param, int(strvalue, 0))
            except KeyError:
                logging.error(f"Error parsing param {param} for {target} from config file")
                raise
            except ValueError:
                logging.error(f"Error parsing param {param} for {target} from config file"
                              "invalid integer")
                raise

    def partition_is_erased(self, partition_num):
        """
        Confirms if the partition is erased by inspecting that the signature has been erased. This
        is more of a sanity check rather than a full confirmation.
        """
        partition_address = \
            self.configstore_base_address + partition_num * (self.configstore_size >> 1)

        logging.debug(f"Confirm partition {partition_num} (0x{partition_address:08x}) is erased.")
        if self._tclif.flash_read_word(partition_address) == MMCONFIG_SIGNATURE:
            raise ProgrammingError(f"Partition {partition_num} was not erased.")

    def erase_flash_sectors(self, start_sector, end_sector):
        logging.info("Erasing config store flash sector...")
        # Must halt target before flash operations can be performed
        self._tclif.reset_halt_target()

        # Erase relevant sector
        self._tclif.flash_erase_sector(self.configstore_bank, start_sector, end_sector)

    def _read_and_verify_partition(self, partition_address, partition_size):
        logging.debug("Reading partition at address %08x", partition_address)
        # Must halt target before flash operations can be performed
        self._tclif.reset_halt_target()

        fd, binfile = tempfile.mkstemp(suffix=".bin")
        os.close(fd)

        for i in range(MAX_READ_ATTEMPTS):
            self.dutif.read_mem(binfile, partition_address, partition_address + partition_size)
            with open(binfile, "rb") as f:
                partition_data = f.read()
            if len(partition_data) == partition_size:
                break

        os.unlink(binfile)
        if len(partition_data) != partition_size:
            if len(partition_data) == 0:
                logging.error("Error reading flash, check gdb parameters.")
                logging.error("Also check that no other instances of gdb are running.")
            raise ConfigStoreReadError(
                f"Read failed (too short; got {len(partition_data)}, expected {partition_size})")

        class Istream:
            def __init__(self, partition_data):
                self._partition_data = partition_data
                self._offset = 0

            def read_u32(self):
                if self._offset + 4 > len(self._partition_data):
                    raise ConfigStoreReadError("Overrun")
                val = struct.unpack("<I", self._partition_data[self._offset:self._offset + 4])[0]
                self._offset += 4
                return val

            def read_u16(self):
                if self._offset + 2 > len(self._partition_data):
                    raise ConfigStoreReadError("Overrun")
                val = struct.unpack("<H", self._partition_data[self._offset:self._offset + 2])[0]
                self._offset += 2
                return val

            def read_u8(self):
                val = self._partition_data[self._offset]
                self._offset += 1
                return val

            def read(self, length):
                if self._offset + length > len(self._partition_data):
                    raise ConfigStoreReadError("Overrun")
                val = self._partition_data[self._offset:self._offset + length]
                self._offset += length
                return val

            def is_at_eof(self):
                return self._offset >= len(self._partition_data)

        return ConfigStorePartition.deserialize(Istream(partition_data))

    def read_config_store(self):
        partitions = [None, None]
        partition_size = (self.configstore_size >> 1)
        for i, offset in enumerate([0, partition_size]):
            address = self.configstore_base_address + offset
            try:
                partitions[i] = self._read_and_verify_partition(address, partition_size)
            except ConfigStoreDeserializeError as e:
                logging.warning(
                    f"Failed to read config store partition {i} at 0x{address:08x}: {e}")
        return ConfigStore(partitions)

    def dump_config_store_binary(self, filename):
        self.dutif.read_mem(filename, self.configstore_base_address,
                            self.configstore_base_address + self.configstore_size)

    def write_config_store(self, config_store):
        if not config_store.has_staged_changes():
            logging.info("Skipping update as no change in config store")
            return

        logging.info("Generate config store image...")

        num_partition_sectors = (
            self.configstore_last_sector - self.configstore_first_sector + 1) >> 1

        partition_num_to_write = config_store.oldest_partition_num()
        partition_address = \
            self.configstore_base_address + partition_num_to_write * (self.configstore_size >> 1)
        partition_first_sector = \
            self.configstore_first_sector + partition_num_to_write * num_partition_sectors
        partition_last_sector = partition_first_sector + num_partition_sectors - 1

        self.erase_flash_sectors(partition_first_sector, partition_last_sector)
        logging.debug("Erasing partition: first sector %d, last sector %d",
                      partition_first_sector, partition_last_sector)

        # Run sanity check on config store before writing
        config_store.sanity_check()

        fd, binfile = tempfile.mkstemp(suffix=".bin")
        with os.fdopen(fd, "wb") as f:
            config_store.staging.serialize(f)

        fd, elffile = tempfile.mkstemp(suffix=".elf")
        os.close(fd)
        subprocess.run([
            f"{self.toolchain_prefix}objcopy",
            "-I", "binary", "-O", "elf32-littlearm",
            "--change-section-vma", f".data=0x{partition_address:08x}",
            "--change-section-lma", f".data=0x{partition_address:08x}",
            binfile, elffile
        ], check=True)

        try:
            self.partition_is_erased(partition_num_to_write)
        except ProgrammingError as e:
            logging.error(f"Failed to erase before attempting load, {repr(e)}")
            raise

        # Write to Flash
        logging.info("Writing partition %d to flash at 0x%08x...",
                     partition_num_to_write, partition_address)
        self.dutif.load(elffile)
        self.dutif.reset_halt()

        logging.debug("binfile: %s, elffile %s", binfile, elffile)
        os.unlink(binfile)
        os.unlink(elffile)

        try:
            partition_size = (self.configstore_size >> 1)
            config_store.update_partition(
                partition_num_to_write,
                self._read_and_verify_partition(partition_address, partition_size))
        except ConfigStoreDeserializeError as e:
            logging.error(f"Validation error: {e}")
            sys.exit(1)
        logging.info("Flash written successfully")


class ConfigStoreFileProgrammer:
    def __init__(self, toolchain_prefix, filename):
        self.filename = filename
        self.toolchain_prefix = toolchain_prefix
        self.cache = None

    def __enter__(self):
        self.filehandle = open(self.filename, "wb")
        return self

    def __exit__(self, type, value, tb):
        if self.filehandle:
            self.filehandle.close()

    def partition_is_erased(self, _partition_num):
        return True

    def erase_flash_sectors(self, start_sector, end_sector):
        pass

    def read_config_store(self):
        return ConfigStore([None, None])

    def write_config_store(self, config_store):
        if not config_store.has_staged_changes():
            logging.info("Skipping update as no change in config store")
            return

        logging.info("Generate config store image...")

        config_store.sanity_check()

        config_store.staging.serialize(self.filehandle)

        # Move staging contents to partition 0 so that we can subsequently dump the contents.
        config_store.update_partition(0, config_store.staging)


def _expand_filename_in_json(json_filename, filename):
    unexpanded_filename = os.path.expanduser(filename)
    filename = os.path.expandvars(unexpanded_filename)
    # Change file path relative to the JSON file, but only if it did not begin
    # with an environment variable. Note that if it begin with an env var then
    # the first character must have changed, since env vars start with $ or %.
    if unexpanded_filename[0] == filename[0] and not os.path.isabs(filename):
        if json_filename == "-":
            logging.error(
                "File references in JSON specified by stdin must have absolute paths")
            sys.exit(1)
        filename = os.path.join(os.path.dirname(json_filename), filename)
    return filename


def _app_setup_args(parser):
    parser.add_argument("-v", "--verbose",
                        action="count",
                        default=0,
                        help="Increase verbosity of log messages (repeat for increased verbosity)")
    parser.add_argument("-l", "--log-file",
                        help="Log to the given file as well as to the console")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("-H", "--debug-host",
                       help="Hostname of machine on which OpenOCD is running")
    group.add_argument("-o", "--output-file",
                       help="Write output to a binary file instead of loading to a device")
    parser.add_argument("-g", "--gdb-port",
                        default=DEFAULT_GDB_PORT,
                        type=int,
                        help="OpenOCD GDB port to use (only applies if -H flag is specified)")
    parser.add_argument("-t", "--tcl-port",
                        default=DEFAULT_TCL_PORT,
                        type=int,
                        help="OpenOCD TCL port to use (only applies if -H flag is specified)")
    parser.add_argument("-C", "--config-json", default=f"{SCRIPT_DIR}/configstore.json",
                        help="Specifies the path to the configstore.json configuration file.")
    parser.add_argument("-p", "--platform",
                        help="""
                        Specifies platform configuration to be used. If left blank the script will
                        attempt to auto-detect the configuration to use.
                        (Only applies if -H flag is specified.)
                        """)
    parser.add_argument("-T", "--toolchain-prefix",
                        default=DEFAULT_TOOLCHAIN_PREFIX,
                        help="Specifies the prefix that will be applied to objcopy and gdb")
    parser.add_argument("-d", "--dump",
                        action="store_true",
                        help="If specified, dumps the contents of config store")
    parser.add_argument("-D", "--dump-binary",
                        help="If specified, dumps the binary contents to the given file")
    parser.add_argument("-b", "--bcf",
                        help="Specifies the path to the BCF that is to be loaded into "
                             "the configstore. ***DEPRECATED***, use write-file instead.")

    subparsers = parser.add_subparsers(help="sub-command help",
                                       dest="command")

    parser_erase = subparsers.add_parser("erase",
                                         help="Erases existing keys from config store")
    parser_erase.add_argument("-a", "--all",
                              action="store_true",
                              help="Does a low level format for the config store "
                              "clearing all metadata and backup copies")
    parser_erase.set_defaults(func=_handle_erase)

    parser_write_json = subparsers.add_parser("write-json",
                                              help="Write values from json to config store")
    parser_write_json.add_argument("json", nargs="+",
                                   help="JSON (or HJSON) file containing the key/value pairs "
                                   "to be written, or - to read from stdin. "
                                   "Files referred to in the JSON are relative to the location "
                                   "of the JSON file unless the path begins with an environment "
                                   "variable. When reading from stdin file paths "
                                   "must be absolute.")
    parser_write_json.set_defaults(func=_handle_write_json)

    parser_write_string = subparsers.add_parser("write-string",
                                                help="Write a single string entry to mmconfig")
    parser_write_string.add_argument("key", help="The key in configstore to load the value to")
    parser_write_string.add_argument("value",
                                     help="The string value to write to the specified key")
    parser_write_string.set_defaults(func=_handle_write_string)

    parser_read_string = subparsers.add_parser("read-string",
                                               help="Reads the key as a string to stdout")
    parser_read_string.add_argument("key", help="The key in configstore to read")
    parser_read_string.set_defaults(func=_handle_read_string)

    parser_write_string = subparsers.add_parser("write-hex",
                                                help="Write a single hex-string entry to mmconfig")
    parser_write_string.add_argument("key", help="The key in configstore to load the value to")
    parser_write_string.add_argument("value",
                                     help="The hex-string value to write to the specified key")
    parser_write_string.set_defaults(func=_handle_write_hex)

    parser_read_string = subparsers.add_parser("read-hex",
                                               help="Reads the value as a hex-string to stdout")
    parser_read_string.add_argument("key", help="The key in configstore to read")
    parser_read_string.set_defaults(func=_handle_read_hex)

    parser_write_file = subparsers.add_parser("write-file",
                                              help="Write an entire file to the config store key")
    parser_write_file.add_argument("key", help="The key in configstore to load the file to")
    parser_write_file.add_argument("file",
                                   help="The name of the file whose contents should be loaded")
    parser_write_file.set_defaults(func=_handle_write_file)

    parser_delete_key = subparsers.add_parser("delete-key",
                                              help="Deletes the specified key only")
    parser_delete_key.add_argument("key", help="The key in configstore to delete")
    parser_delete_key.set_defaults(func=_handle_delete_key)


def _handle_erase(args, programmer):
    if args.all:
        # Erase both partitions as requested - this is fast as we don't read
        programmer.erase_flash_sectors(programmer.configstore_first_sector,
                                       programmer.configstore_last_sector)

        try:
            programmer.partition_is_erased(0)
            programmer.partition_is_erased(1)
        except ProgrammingError as e:
            logging.error(f"Failed to confirm flash erase, {repr(e)}")
            raise

        if args.dump:
            logging.warning("Dump skipped as configstore is completely "
                            "erased if `erase --all` is specified.")
            sys.exit(0)
        return None
    else:
        config_store = programmer.read_config_store()
        # Clear the config store, but retain the version and backup image
        config_store.staging._dictionary = {}
        programmer.write_config_store(config_store)
        return config_store


def _handle_write_json(args, programmer):
    config_store = programmer.read_config_store()

    for json_filename in args.json:
        if json_filename != "-":
            with open(json_filename, "r") as f:
                new_entries = hjson.load(f)
        else:
            new_entries = hjson.load(sys.stdin)

        for data_type, data in new_entries.items():
            if data_type == "strings":
                for k, v in data.items():
                    config_store.staging.add_entry_string(k, v)
            elif data_type == "files":
                for k, v in data.items():
                    expanded_filename = _expand_filename_in_json(json_filename, v)
                    config_store.staging.add_entry_file(k, expanded_filename)
            elif data_type == "hex_strings":
                for k, v in data.items():
                    config_store.staging.add_entry_hex_string(k, v)
            else:
                logging.warning(f"Data type '{data_type}' is unknown, skipping.")

    programmer.write_config_store(config_store)
    logging.info("JSON contents loaded to config store successfully")

    return config_store


def _handle_write_string(args, programmer):
    config_store = programmer.read_config_store()

    config_store.staging.add_entry_string(args.key, args.value)

    programmer.write_config_store(config_store)
    logging.info("Key/Value programmed successfully")

    return config_store


def _handle_read_string(args, programmer):
    config_store = programmer.read_config_store()

    data = config_store.staging.read_entry(args.key)
    if data[len(data) - 1] == 0:
        # Strip trailing NULL
        print(data[:len(data) - 1].decode("utf-8"))
    else:
        logging.error("Not a string value")

    return config_store


def _handle_write_hex(args, programmer):
    config_store = programmer.read_config_store()

    config_store.staging.add_entry_hex_string(args.key, args.value)

    programmer.write_config_store(config_store)
    logging.info("Key/Value programmed successfully")

    return config_store


def _handle_read_hex(args, programmer):
    config_store = programmer.read_config_store()

    data = config_store.staging.read_entry(args.key)
    print(f"length: {len(data)} bytes")
    print(f"data: 0x{data.hex()}")

    return config_store


def _handle_write_file(args, programmer):
    config_store = programmer.read_config_store()

    config_store.staging.add_entry_file(args.key, args.file)
    programmer.write_config_store(config_store)
    logging.info("File loaded to config store successfully")

    return config_store


def _handle_delete_key(args, programmer):
    config_store = programmer.read_config_store()

    config_store.staging.delete_entry(args.key)
    programmer.write_config_store(config_store)
    logging.info(f"Key ({args.key}) deleted successfully")

    return config_store


def _read_config_store(_args, programmer):
    config_store = programmer.read_config_store()
    return config_store


def _app_main(args):
    if args.debug_host is not None:
        programmer_class = ConfigStoreOpenOCDProgrammer
        logging.debug("Loading platform configuration from %s", args.config_json)
        with open(args.config_json, "r") as f:
            platform_configstore_args = hjson.load(f)
        if args.platform is not None:
            supported_platforms = set()
            for v in platform_configstore_args.values():
                supported_platforms.update(v["supported_platforms"])
            if args.platform not in supported_platforms:
                logging.error("Unsupported platform %s. Supported platforms:", args.platform)
                for platform in sorted(supported_platforms):
                    logging.error("    - %s", platform)
                sys.exit(2)

        programmer_args = [
            args.debug_host, args.gdb_port, args.tcl_port, platform_configstore_args, args.platform
        ]
    else:
        programmer_class = ConfigStoreFileProgrammer
        programmer_args = [args.output_file]

    with programmer_class(args.toolchain_prefix, *programmer_args) as programmer:
        if args.bcf:
            # This is a special case kept for backwards compatibility. The write-file subparser is
            # the preferred method.
            config_store = programmer.read_config_store()

            config_store.staging.add_entry_file("BCF_FILE", args.bcf)
            programmer.write_config_store(config_store)
            logging.info("BCF file programmed successfully.")
        else:
            try:
                operation_fn = args.func
            except AttributeError:
                if not args.dump and not args.dump_binary:
                    logging.error("You likely forgot to specify a sub-command.")
                    sys.exit(1)

                operation_fn = _read_config_store
            try:
                config_store = operation_fn(args, programmer)
            except Exception as e:
                logging.error("Failed to execute %s: %s", operation_fn.__name__, str(e))
                if args.verbose >= 2:
                    traceback.print_exc()
                sys.exit(1)

        if config_store is not None:
            if args.dump:
                logging.info("Dumping contents of config store")
                config_store.dump()

            if args.dump_binary:
                logging.info("Dumping binary contents of config store to %s", args.dump_binary)
                programmer.dump_config_store_binary(args.dump_binary)


def _main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
                                     description=__doc__)
    _app_setup_args(parser)

    args = parser.parse_args()

    # Configure logging
    log_handlers = [logging.StreamHandler()]
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))

    LOG_FORMAT = "%(asctime)s %(levelname)s: %(message)s"
    if args.verbose >= 2:
        logging.basicConfig(level=5, format=LOG_FORMAT, handlers=log_handlers)
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
