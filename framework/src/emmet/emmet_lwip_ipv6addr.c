/*
 * Copyright (c) 2010 Inico Technologies Ltd.
 * Copyright 2023 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Ivan Delamer <delamer@inicotech.com>
 *
 * Functions for handling IPv6 addresses.
 *
 * Please coordinate changes and requests with Ivan Delamer
 * <delamer@inicotech.com>
 */

#include <endian.h>
#include "emmet_private.h"

static inline bool emmet_isxdigit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return true;
    }
    if (c >= 'a' && c <= 'f')
    {
        return true;
    }
    if (c >= 'A' && c <= 'F')
    {
        return true;
    }
    return false;
}

int emmet_ip6addr_aton(const char *cp, uint32_t *addr)
{
    uint32_t addr_index, zero_blocks, current_block_index, current_block_value;
    const char *s;

    MMOSAL_ASSERT(addr != NULL);

    zero_blocks = 8;
    for (s = cp; *s != 0; s++)
    {
        if (*s == ':')
        {
            zero_blocks--;
        }
        else if (!emmet_isxdigit(*s))
        {
            break;
        }
    }

    addr_index = 0;
    current_block_index = 0;
    current_block_value = 0;
    for (s = cp; *s != 0; s++)
    {
        if (*s == ':')
        {
            if (current_block_index & 0x1)
            {
                addr[addr_index++] |= current_block_value;
            }
            else
            {
                addr[addr_index] = current_block_value << 16;
            }

            current_block_index++;
            current_block_value = 0;
            if (current_block_index > 7)
            {
                return 0;
            }
            if (s[1] == ':')
            {
                if (s[2] == ':')
                {
                    return 0;
                }
                s++;
                while (zero_blocks > 0)
                {
                    zero_blocks--;
                    if (current_block_index & 0x1)
                    {
                        addr_index++;
                    }
                    else
                    {
                        addr[addr_index] = 0;
                    }
                    current_block_index++;
                    if (current_block_index > 7)
                    {
                        return 0;
                    }
                }
            }
        }
        else if (*s >= '0' && *s <= '9')
        {
            current_block_value = (current_block_value << 4) | (uint32_t)(*s - '0');
        }
        else if (*s >= 'a' && *s <= 'f')
        {
            current_block_value = (current_block_value << 4) | (uint32_t)(*s - 'a' + 0x0a);
        }
        else if (*s >= 'A' && *s <= 'F')
        {
            current_block_value = (current_block_value << 4) | (uint32_t)(*s - 'A' + 0x0a);
        }
        else
        {
            break;
        }
    }

    if (addr)
    {
        if (current_block_index & 0x1)
        {
            addr[addr_index++] |= current_block_value;
        }
        else
        {
            addr[addr_index] = current_block_value << 16;
        }
        for (addr_index = 0; addr_index < 4; addr_index++)
        {
            addr[addr_index] = htobe32(addr[addr_index]);
        }
    }

    if (current_block_index != 7)
    {
        return 0;
    }

    return 1;
}
