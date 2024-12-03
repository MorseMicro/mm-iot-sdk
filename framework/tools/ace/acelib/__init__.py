#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import ipaddress

from acelib.dutif import DutIf  # noqa: F401
from acelib.enumprovider import AceEnumProvider

ACE_ENUM = AceEnumProvider()


def ip_addr_to_bytes(ipaddr_str):
    return ipaddress.ip_address(ipaddr_str).packed


def ip_addr_bytes_to_str(ip_addr):
    return str(ipaddress.ip_address(ip_addr))


def mac_addr_bytes_to_str(mac_addr):
    mac_addr = b"\0\0\0\0\0\0" if mac_addr is None else mac_addr
    return (":".join([f"{octet:02x}" for octet in mac_addr]))


def mac_addr_str_to_bytes(mac_addr_str):
    return bytes([int(x, 16) for x in mac_addr_str.split(":")])
