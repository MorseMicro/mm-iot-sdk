/*
 * Copyright 2023 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#include "emmet_private.h"
#include "emmet-api.pb-c.h"

void emmet_pack_filersp(struct emmet_raw_rsp *raw_rsp, Emmet__FileRsp *filersp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.file = filersp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_FILE;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void handle_read(struct emmet_raw_rsp *raw_rsp, Emmet__FileReadCmd *read_cmd)
{
    Emmet__FileReadRsp read_rsp = EMMET__FILE_READ_RSP__INIT;
    Emmet__FileRsp filersp = EMMET__FILE_RSP__INIT;

    uint8_t *buffer = NULL;
    read_rsp.payload.data = NULL;
    read_rsp.payload.len = 0;

    do {
        int fd = open(read_cmd->path, O_RDONLY);
        if (fd < 0)
        {
            read_rsp.len = -errno;
            break;
        }

        if (lseek(fd, read_cmd->offset, SEEK_SET) < 0)
        {
            close(fd);
            read_rsp.len = -ESPIPE;
            break;
        }

        buffer = (uint8_t *)mmosal_malloc(read_cmd->len);
        if (buffer == NULL)
        {
            close(fd);
            read_rsp.len = -ENOMEM;
            break;
        }

        read_rsp.len = read(fd, (void *)buffer, read_cmd->len);
        if (read_rsp.len >= 0)
        {
            read_rsp.payload.data = buffer;
            read_rsp.payload.len = read_rsp.len;
        }
        else
        {
            read_rsp.len = -errno;
        }

        close(fd);
    } while (0);

    filersp.read = &read_rsp;
    filersp.rsp_case = EMMET__FILE_RSP__RSP_READ;
    emmet_pack_filersp(raw_rsp, &filersp);

    if (buffer)
    {
        mmosal_free(buffer);
    }
}

static void handle_write(struct emmet_raw_rsp *raw_rsp, Emmet__FileWriteCmd *write_cmd)
{
    int fd = open(write_cmd->path, O_WRONLY | O_APPEND | O_CREAT, 0);
    if (fd < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
        return;
    }

    if (write(fd, (void *)write_cmd->payload.data, write_cmd->payload.len) < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
        close(fd);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    close(fd);
}

static void handle_del(struct emmet_raw_rsp *raw_rsp, Emmet__FileDelCmd *delete_cmd)
{
    if (unlink(delete_cmd->path) < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
}

void emmet_handle_filecmd(struct emmet_raw_rsp *raw_rsp, Emmet__FileCmd *filecmd)
{
    switch (filecmd->cmd_case)
    {
        case EMMET__FILE_CMD__CMD_READ:
            handle_read(raw_rsp, filecmd->read);
            break;

        case EMMET__FILE_CMD__CMD_WRITE:
            handle_write(raw_rsp, filecmd->write);
            break;

        case EMMET__FILE_CMD__CMD_DEL:
            handle_del(raw_rsp, filecmd->del);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
