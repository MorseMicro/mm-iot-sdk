/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "cli/autogen/mmagic_cli_internal.h"

struct mmagic_cli_config_elem *mmagic_cli_element_search(struct mmagic_cli_config_elem *elements,
                                                         size_t num, const char *name)
{
    size_t low = 0;
    size_t high = num;
    while (low < high)
    {
        size_t mid = low + ((high - low) / 2);
        int c = strcmp(elements[mid].name, name);
        if (c == 0)
        {
            return &elements[mid];
        }
        if (c < 0)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    /* Unable to find entry */
    return NULL;
}

void mmagic_cli_printf(EmbeddedCli *cli, const char *format, ...)
{
    char string[MMAGIC_CLI_PRINT_BUF_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(string, sizeof(string), format, args);
    va_end(args);
    embeddedCliPrint(cli, string);
}
