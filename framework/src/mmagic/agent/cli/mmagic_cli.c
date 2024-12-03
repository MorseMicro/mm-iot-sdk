/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmagic_cli.h"
#include "cli/autogen/mmagic_cli_wlan.h"
#include "cli/autogen/mmagic_cli_ip.h"
#include "cli/autogen/mmagic_cli_ping.h"
#include "cli/autogen/mmagic_cli_iperf.h"
#include "cli/autogen/mmagic_cli_sys.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli.def"

/* In one (and only one) compilation unit (.c/.cpp file) define macro to unwrap implementations */
#define EMBEDDED_CLI_IMPL
#include "cli/embedded_cli.h"

static void mmagic_cli_writeChar(EmbeddedCli *cli, char c)
{
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    ctx->tx_cb(&c, 1, ctx->tx_cb_arg);
}

void mmagic_cli_rx(struct mmagic_cli *ctx, const char *buf, size_t num)
{
    for (; num > 0; num--)
    {
        embeddedCliReceiveChar(ctx->cli, *buf++);
    }
    embeddedCliProcess(ctx->cli);
}

void mmagic_cli_get(EmbeddedCli *cli, char *args, void *context)
{
    (void)context;
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    const char *var = embeddedCliGetToken(args, 1);
    if (var == NULL)
    {
        embeddedCliPrint(cli, "Invalid argument");
        return;
    }

    if (ctx->config_accessors == NULL)
    {
        embeddedCliPrint(cli, "No config variables accessible");
        return;
    }

    struct mmagic_cli_config_accessor *accessor;
    if (!strcmp("all", var))
    {
        for (accessor = ctx->config_accessors;
             accessor != NULL; accessor = accessor->next)
        {
            accessor->get(ctx, ctx->cli, var);
        }
        return;
    }

    char *variable;
    char *dot = strstr(var, ".");
    size_t module_len;
    if (dot)
    {
        variable = dot + 1;
        module_len = dot - var;
    }
    else
    {
        variable = "all";
        module_len = strlen(var);
    }

    for (accessor = ctx->config_accessors;
         accessor != NULL; accessor = accessor->next)
    {
        if (strlen(accessor->name) == module_len && !strncmp(accessor->name, var, module_len))
        {
            accessor->get(ctx, ctx->cli, variable);
            break;
        }
    }
}

void mmagic_cli_set(EmbeddedCli *cli, char *args, void *context)
{
    (void)context;
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens < 2)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }

    const char *var = embeddedCliGetToken(args, 1);
    const char *val = embeddedCliGetToken(args, 2);

    char *variable;
    char *dot = strstr(var, ".");
    size_t module_len;
    if (dot)
    {
        variable = dot + 1;
        module_len = dot - var;
    }
    else
    {
        embeddedCliPrint(cli, "Invalid specifier for config variable");
        return;
    }

    struct mmagic_cli_config_accessor *accessor;
    for (accessor = ctx->config_accessors;
         accessor != NULL; accessor = accessor->next)
    {
        if (strlen(accessor->name) == module_len && !strncmp(accessor->name, var, module_len))
        {
            accessor->set(ctx, ctx->cli, variable, val);
            break;
        }
    }
}

void mmagic_cli_commit(EmbeddedCli *cli, char *args, void *context)
{
    (void)context;
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens < 1)
    {
        embeddedCliPrint(cli, "At least 1 argument required to commit");
        return;
    }

    if (ctx->config_accessors == NULL)
    {
        embeddedCliPrint(cli, "No config variables accessible");
        return;
    }

    int i;
    for (i = 1; i <= num_tokens; i++)
    {
        const char *var = embeddedCliGetToken(args, i);

        /* Sanity check, should not be NULL */
        MMOSAL_ASSERT(var);

        struct mmagic_cli_config_accessor *accessor;
        if (!strcmp("all", var))
        {
            for (accessor = ctx->config_accessors;
                 accessor != NULL; accessor = accessor->next)
            {
                accessor->commit(ctx, ctx->cli, var);
            }
            return;
        }

        char *variable;
        char *dot = strstr(var, ".");
        size_t module_len;
        if (dot)
        {
            variable = dot + 1;
            module_len = dot - var;
        }
        else
        {
            variable = "all";
            module_len = strlen(var);
        }

        for (accessor = ctx->config_accessors;
             accessor != NULL; accessor = accessor->next)
        {
            if (!strncmp(accessor->name, var, module_len))
            {
                accessor->commit(ctx, cli, variable);
                break;
            }
        }
    }
}

static void mmagic_cli_register_default_bindings(struct mmagic_cli *ctx)
{
    embeddedCliAddBinding(ctx->cli, (CliCommandBinding) {
        "get",
        "Retrieves the value of a given config variable, `all` to list all available.",
        true,
        ctx,
        mmagic_cli_get
    });

    embeddedCliAddBinding(ctx->cli, (CliCommandBinding) {
        "set",
        "Set the value of the variable specified.",
        true,
        ctx,
        mmagic_cli_set
    });

    embeddedCliAddBinding(ctx->cli, (CliCommandBinding) {
        "commit",
        "Commits the specified variables to persistent storage, use 'all' to commit everything.",
        true,
        ctx,
        mmagic_cli_commit
    });
}

/**
 * Helper function to insert a config accessor into the @c config_accessors linked list in the cli
 * data struct.
 *
 * @param ctx      Reference to the global mmagic context
 * @param accessor Reference to the accessor to be added. This function will take ownership.
 *
 * @note It is assumed that @c accessor->next is NULL. i.e. it is a single item and not a list.
 */
void mmagic_cli_register_config_accessor(struct mmagic_cli *ctx,
                                         struct mmagic_cli_config_accessor *accessor)
{
    accessor->next = ctx->config_accessors;
    ctx->config_accessors = accessor;
}

static struct mmagic_cli cli_ctx;

struct mmagic_cli *mmagic_cli_init(const struct mmagic_cli_init_args *args)
{
    struct mmagic_cli *ctx = &cli_ctx;
    memset(&cli_ctx, 0, sizeof(cli_ctx));

    mmosal_safer_strcpy(ctx->core.app_version, args->app_version, sizeof(ctx->core.app_version));
    ctx->core.set_deep_sleep_mode_cb = args->set_deep_sleep_mode_cb;
    ctx->core.set_deep_sleep_mode_cb_arg = args->set_deep_sleep_mode_cb_arg;
    ctx->core.event_fn = mmagic_cli_handle_event;
    ctx->core.event_fn_arg = ctx;
    ctx->tx_cb = args->tx_cb;
    ctx->tx_cb_arg = args->tx_cb_arg;

    /* Disable deep sleep on startup to ensure we stay awake to receive data */
    if (ctx->core.set_deep_sleep_mode_cb != NULL)
    {
        ctx->core.set_deep_sleep_mode_cb(MMAGIC_DEEP_SLEEP_MODE_DISABLED,
                                         ctx->core.set_deep_sleep_mode_cb_arg);
    }

    mmagic_core_init_modules(&ctx->core);

    ctx->config = embeddedCliDefaultConfig();
    ctx->config->rxBufferSize = MMAGIC_CLI_RX_BUFFER_SIZE;
    ctx->config->cmdBufferSize = MMAGIC_CLI_CMD_BUFFER_SIZE;
    ctx->config->historyBufferSize = MMAGIC_CLI_HISTORY_SIZE;
    ctx->config->maxBindingCount = MMAGIC_CLI_BINDING_COUNT;
    ctx->config->cliBufferSize = embeddedCliRequiredSize(ctx->config);
    ctx->config->cliBuffer = (uint32_t *)mmosal_malloc(ctx->config->cliBufferSize);
    MMOSAL_ASSERT(ctx->config->cliBuffer != NULL);

    ctx->cli = embeddedCliNew(ctx->config);
    if (ctx->cli == NULL)
    {
        printf("Failed to init CLI\n");
        return NULL;
    }

    ctx->cli->writeChar = mmagic_cli_writeChar;
    ctx->cli->appContext = ctx;

    mmagic_cli_register_default_bindings(ctx);
    mmagic_cli_init_modules(ctx);

    embeddedCliPrint(ctx->cli, "\n\nMorse Micro CLI (Built " __DATE__ " " __TIME__ ")\n\n");

    return ctx;
}
