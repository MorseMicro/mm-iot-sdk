#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import argparse
import logging
import os
import re
import yaml
from typing import List, Optional
from datetime import datetime
from dataclasses import dataclass, field
from dacite import from_dict

STD_C_TYPES = ['uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
               'int16_t', 'bool', 'char', 'int32_t']

''' Maximum allowed length of any config name, this includes the newline character '''
MAX_CONFIG_NAME_LEN = 32

TYPE_ARRAY_REGEX = re.compile(r'^array(?P<num>\d{1,4})_(?P<type>.*)')
STRUCT_NAME_REGEX = re.compile(r'^struct_[\w\d]*$')
ENUM_NAME_REGEX = re.compile(r'^enum_[\w\d]*$')


def validate_name(name: str, regex: re.Pattern):
    ''' Given a name and a regex, validate that the name matches the pattern. '''
    if regex.match(name):
        return

    raise Exception(f'Incorrect format for object {name}, reqex: {str(regex)}')


# ---------- ENUM ---------- #
@dataclass(frozen=True)
class Value:
    name: str
    description: str
    value: int


@dataclass(frozen=True)
class Enum:
    name: str
    description: str
    values: List[Value]

    def human_readable_name(self):
        readable_name = " ".join(self.name.split("_")[1:])
        return readable_name.capitalize()

    def __post_init__(self):
        stripped_name = re.sub('enum_(?P<name>.*)', '\\g<name>', self.name)
        object.__setattr__(self, 'datatype', f'enum mmagic_{stripped_name}')
        validate_name(self.name, ENUM_NAME_REGEX)


# ---------- STRUCT ---------- #
@dataclass(frozen=True)
class Element:
    name: str
    description: str
    type: str


@dataclass(frozen=True)
class Struct:
    name: str
    description: str
    elements: List[Element]

    def __post_init__(self):
        object.__setattr__(self, 'datatype', f'struct {self.name}')
        object.__setattr__(self, 'decl_datatype', f'struct MM_PACKED {self.name}')
        validate_name(self.name, STRUCT_NAME_REGEX)


# ---------- Module ---------- #
@dataclass(frozen=False)
class ConfigVar:
    name: str
    description: str
    type: str


@dataclass(frozen=True)
class Argument:
    name: str
    description: str
    type: str


@dataclass(frozen=True)
class Command:
    name: str
    description: str
    stream_type: bool = False
    command_args: List[Argument] = field(default_factory=list)
    response_args: List[Argument] = field(default_factory=list)


@dataclass(frozen=True)
class Event:
    name: str
    id: int
    description: str
    event_args: List[Argument] = field(default_factory=list)

@dataclass(frozen=False)
class Module:
    id: int
    description: str
    name: str
    cli_support: bool = True
    configs: List[ConfigVar] = field(default_factory=list)
    commands: List[Command] = field(default_factory=list)
    events: List[Event] = field(default_factory=list)

    def _validate_config_name_lengths(self, max_length: int):
        ''' Validate that a given config name string doesn't exceed the max length. '''
        for config in self.configs:
            name = f"{self.name}.{config.name}\n"
            if len(name) >= max_length:
                raise Exception(f'Config string "{name}" length ({len(name)}) exceeds the max length ({max_length})')

    def __post_init__(self):
        self.config = self.configs.sort(key=lambda x: x.name)
        if self.configs is not None:
            self._validate_config_name_lengths(MAX_CONFIG_NAME_LEN)


# ---------- Configuration ---------- #
@dataclass(frozen=True)
class Configuration:
    enums: List[Enum]
    structs: List[Struct]
    modules: List[Module]

    def __post_init__(self):
        object.__setattr__(self, 'copyright_year', datetime.now().year)
        types_used = set()
        for module in self.modules:
            for var in module.configs:
                types_used.add(var.type)
        object.__setattr__(self, 'types_used', sorted(types_used))

    def _find_struct(self, name: str):
        if 'struct' in name:
            try:
                struct = next(x for x in self.structs if x.name == name)
                return struct
            except StopIteration:
                raise Exception(f'Unable to find entry for {name}')

    def _find_enum(self, name: str):
        if ENUM_NAME_REGEX.match(name):
            try:
                enum = next(x for x in self.enums if x.name == name)
                return enum
            except StopIteration:
                raise Exception(f'Unable to find entry for {name}')

    def find_datatype(self, name: str):
        # If this is an array, then extract the name of the data type from the full name
        match = TYPE_ARRAY_REGEX.match(name)
        if match:
            name = match.group("type")

        struct = self._find_struct(name)
        if struct:
            return struct.datatype

        enum = self._find_enum(name)
        if enum:
            return enum.datatype

        if name in STD_C_TYPES:
            return name

        raise Exception(f'Invalid type, {name}')

    def datatype_is_c_type(self, name: str):
        return name in STD_C_TYPES

    def datatype_is_scalar(self, name: str):
        if self.datatype_is_c_type(name):
            return True

        enum = self._find_enum(name)
        if enum:
            return True

        return False

    def datatype_is_string(self, name: str):
        return name.startswith("struct_string_")

    """ Get array element from type if array prefix is present else empty string """
    def get_array_element(self, name: str):
        match = TYPE_ARRAY_REGEX.match(name)
        if match:
            groups = match.groupdict()
            return f'[{groups["num"]}]'
        return ''

    def load(yamlfile):
        return from_dict(data_class=Configuration, data=yaml.load(yamlfile, Loader=yaml.Loader))
