#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import logging
import sys

try:
    from acelib import emmet_api_pb2
except ModuleNotFoundError as e:
    logging.error(e)
    logging.error("Try installing protobuf:")
    logging.error("    pip3 install protobuf")
    sys.exit(1)


class AceEnumSubprovider:
    """
    This class is used by AceEnumProvider and should not be instantiated directly.
    """

    def __init__(self, enum_descriptor):
        self.enum_descriptor = enum_descriptor

    def choices(self):
        return self.enum_descriptor.values_by_name.keys()

    def __getattr__(self, name):
        if name not in self.enum_descriptor.values_by_name:
            raise AttributeError(f"Invalid attribute: {name}")
        return self.enum_descriptor.values_by_name[name].number

    def to_string(self, value):
        return self.enum_descriptor.values_by_number[value].name

    def __str__(self):
        val = ", ".join([x.name for x in self.enum_descriptor.values])
        return f"ACE enumeration {self.enum_descriptor.name} ({val})"


class AceEnumProvider:
    """
    This provides a cleaner interface to access enum values in the protobuf API.
    """

    def __getattr__(self, name):
        if name not in emmet_api_pb2.DESCRIPTOR.enum_types_by_name:
            raise AttributeError(f"Invalid attribute: {name}")
        return AceEnumSubprovider(emmet_api_pb2.DESCRIPTOR.enum_types_by_name[name])
