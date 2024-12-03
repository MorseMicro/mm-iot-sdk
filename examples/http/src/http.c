/*
 * Copyright 2022-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief HTTP server example, with APIs for RESTful interfaces.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file contains examples on how to use the lwIP http server.
 * It supports static and dynamically generated pages.
 *
 * Static pages are specified in the @c fs/ directory, and on compilation will be automatically
 * converted to C arrays which will be embedded into the binary.
 * Currently the file converter is compiled to run only on x86 linux systems. To support other
 * systems, @c htmlgen can be recompiled on your development machine from the mainline lwIP sources.
 *
 * Dynamic pages are specified through the custom file system, @c restfs, and are registered by
 * specifying the URI and handler function in the @c rest_endpoints array.
 *
 * Note that only GET requests are supported at this time.
 *
 * To view the web-page, navigate to
 * @code
 * http://<device_ip>/index.html
 * @endcode
 * on a computer with a route to the Morse Micro IoT device, over HaLow. Alternatively, you can run
 * @code
 * curl http://<device_ip>/index.html
 * @endcode
 * from the AP that the device is connected to.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration.
 */

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"

#include "mmipal.h"
#include "lwip/apps/httpd.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/tcpip.h"

#include "restfs.h"
#include "mm_app_common.h"


#if !defined(LWIP_HTTPD_CGI)
#error "http_rest requires HTTPD CGI"
#endif

/** Buffer to store the string set by cgi_set_string() */
static char cgi_string[32] = {0};

/**
 * Example CGI handler to set a global variable based on query parameters.
 * Query string expected as `value=<string to set>`; e.g., @c /rest/set_string?value=test
 *
 * @param index         The index of the endpoint in the CGI handlers table.
 * @param nparams       Number of elements in @p params provided in the URI.
 * @param params        Parameter names provided in the URI.
 * @param values        Values corresponding to each parameter given in the URI.

 * @returns the URI of the response to return (which can be a REST endpoint).
 *
 * @note Strictly speaking GET requests should not be altering state on the server,
 * but for this example we will use it as a lightweight alternative to POST.
 *
 * See lwIP @c httpd for more information
 */
static const char* cgi_set_string(int index, int nparams, char *params[], char *values[])
{
    int i;

    (void)index;

    for (i = 0; i < nparams; i++)
    {
        if (!strncmp(params[i], "value", sizeof("value")))
        {
            mmosal_safer_strcpy(cgi_string, values[i], sizeof(cgi_string));
            return "success.html";
        }
    }

    return "failed.html";
}

/**
 * Get the string previously set by set_string
 *
 * @param fil   File to write to.
 */
static void rest_ep_getstring(struct restfs_file *fil)
{
    restfs_alloc_buffer(fil, sizeof(cgi_string));

    restfs_write(fil, (const uint8_t *)cgi_string, strlen(cgi_string));
}

/**
 * Example endpoint to return fixed html string
 *
 * @param fil   File to write to.
 */
static void rest_ep_success(struct restfs_file *fil)
{
    static const char successhtml[] = "<html><body>Success</body></html>";

    restfs_write_const(fil, successhtml);
}

/**
 * Example endpoint to return fixed html string
 *
 * @param fil   File to write to.
 */
static void rest_ep_failed(struct restfs_file *fil)
{
    static const char failedhtml[] = "<html><body>Failed</body></html>";

    restfs_write_const(fil, failedhtml);
}

/**
 * Hello world example endpoint
 *
 * @param fil   File to write to.
 */
static void rest_ep_hello(struct restfs_file *fil)
{
    restfs_alloc_buffer(fil, 20);

    restfs_printf(fil, "Hello World");
}

/**
 * Vector table of rest endpoints. Declare the URI and handlers for REST endpoints here.
 *
 * For example, HTTP GET on `<ip address>/rest/example_endpoint`
 */
static const struct rest_endpoint rest_endpoints[] = {
    {"success.html", rest_ep_success},
    {"failed.html", rest_ep_failed},
    {"/rest/hello", rest_ep_hello},
    {"/rest/get_string.txt", rest_ep_getstring},
};

/**
 * Vector table of LWIP CGI endpoints. Declare the URI and handlers for CGI endpoints here
 *
 * Will pass query parameters to function call.
 * For example, `<ip_address>/rest/<endpoint>?queryname=queryval&queryname2=queryval2` ... etc.
 */
static const tCGI cgi_endpoints[] = {
    {"/rest/set_string", cgi_set_string}
};


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nMorse HTTP Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    LOCK_TCPIP_CORE();
    rest_init_endpoints(rest_endpoints, LWIP_ARRAYSIZE(rest_endpoints));
    http_set_cgi_handlers(cgi_endpoints, LWIP_ARRAYSIZE(cgi_endpoints));
    httpd_init();
    UNLOCK_TCPIP_CORE();

    /* We idle till we get a connection */
}
