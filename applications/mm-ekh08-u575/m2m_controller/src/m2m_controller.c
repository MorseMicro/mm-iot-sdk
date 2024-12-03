/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief M2M Controller example application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * The @ref m2m_agent.c and @ref m2m_controller.c example applications demonstrate how to use
 * the MMAGIC M2M interface.
 *
 * # Getting Started {#M2M_GETTING_STARTED}
 *
 * ## Setup
 * To setup this demonstration you will need the following:
 * - One @c mm-ekh08-u575 reference platform (See @ref MMPLATFORMS) - this includes a ST
 *   @c NUCLEO-U575ZI-Q board as the motherboard.
 * - An additional ST @c NUCLEO-U575ZI-Q development board
 * - A Morse Micro Access Point connected to the Internet via Ethernet
 *
 * For the purposes of this demonstration the @c mm-ekh08-u575 will be the agent and the standalone
 * @c NUCLEO-U575ZI-Q board will be the controller. The two boards will communicate over the SPI
 * bus, so connect the @c SPI buses on the two ST @c NUCLEO-U575ZI-Q boards together in a 1:1
 * configuration. Connect them up as shown in the table below:
 *
 * | Pin               | On Agent - @c mm-ekh08-u575 | On Controller - @c NUCLEO-U575ZI-Q |
 * | ----------------- | --------------------------- | ---------------------------------- |
 * | @c M2M_SPI_MOSI   | @c PD4 on connector @c CN9  | @c PD4 on connector @c CN9         |
 * | @c M2M_SPI_MISO   | @c PD3 on connector @c CN9  | @c PD3 on connector @c CN9         |
 * | @c M2M_SPI_SCK    | @c PB13 on connector @c CN7 | @c PB13 on connector @c CN7        |
 * | @c M2M_SPI_NSS    | @c PB9 on connector @c CN7  | @c PB9 on connector @c CN7         |
 * | @c GND            | Any convenient @c GND pin   | Any convenient @c GND pin          |
 *
 * For an example of M2M communications over UART, see the @ref cli.c application which has an
 * M2M mode.
 *
 * ## Building
 * The @c Wi-Fi SSID and password are hard coded in @ref m2m_controller.c as this application
 * is very basic and does not support saving settings using persistent store. The controller
 * application is kept simple to allow easy porting of the controller to any platform. Update
 * @c SSID and @c SAE_PASSPHRASE as appropriate for your test setup and build the @c m2m_agent
 * and the @c m2m_controller applications as shown in @ref BUILDING_FIRMWARE.
 *
 * ## Running
 * - Load the built @c m2m_agent.elf firmware into the @c mm-ekh08-u575 device as explained in
 *   @ref PROGRAMMING_FIRMWARE. Ensure you have loaded the Board Configuration File (BCF) and set
 *   the 2 letter country code in the @c wlan.country_code setting in persistent storage as
 *   described in @ref SET_APP_CONFIGURATION.
 * - Execute the @c m2m_agent application by resetting the device or issuing the @c continue
 *   command in @c gdb. You should see the following message on the console:
 * @code
 * M2M Agent Example (Built May  8 2024 17:06:40)
 *
 * M2M interface enabled
 * @endcode
 * - Load the built @c m2m_controller.elf firmware into the bare ST @c NUCLEO-U575ZI-Q as
 *   explained in @ref PROGRAMMING_FIRMWARE. Note that the controller does not have any
 *   persistent storage and so you do not need to load any BCF files or settings into
 *   persistent storage as you would do with proper @ref MMPLATFORMS.
 * - Execute the @c m2m_controller application by resetting the device or issuing the
 *   @c continue command in @c gdb. You should see the following message on the console:
 * @code
 * M2M Controller Example (Built May  9 2024 09:12:18)
 *
 * M2M Controller enabled. Awaiting Agent start
 * MMAGIC_LLC: Received agent START event!
 * Agent start notification received
 *
 *
 * #### Example WLAN Connect using MMAGIC Controller ####
 *
 * Attempting to connect to MHS_Test with passphrase 12345678
 * This may take some time (~30 seconds)
 * Link Up
 * Link is up (DHCP). Time: 10483 ms, IP: 192.168.1.189, Netmask: 255.255.255.0, Gateway:
 * 192.168.1.1
 * WLAN connection established
 * ...
 * <output continues>
 * ...
 * @endcode
 *
 * @note
 * - If the controller gets stuck at the following message, check the connections and
 *   try restarting the @c m2m_agent application.
 * @code
 * M2M Controller enabled. Awaiting Agent start
 * @endcode
 * - If the controller gets an error after the following message, check the access point can
 *   connect to the internet and is setup for bridging or routing.
 * @code
 * #### Example TCP Client using MMAGIC Controller ####
 * @endcode
 *
 * ## Testing the TCP Echo Server
 * In addition to an example TCP client that connects to the @c morsemicro.com web server and
 * the beacon monitor example, the @c m2m_controller also demonstrates how to use TCP server
 * sockets to serve multiple incoming TCP client connections.
 *
 * ### Using Telnet ###
 * - Note the IP address assigned to the system (Agent + Controller is the system) in the Link Up
 *   message displayed by the controller.
 * - When you see the following message in the console it means the system is ready to accept
 *   incoming connections on port 5000:
 * @code
 * #### Example TCP Echo Server using MMAGIC Controller ####
 *
 * Opened TCP socket on stream_id 1
 * @endcode
 * - Now log in to the access point using @c ssh (Alternatively, you can do this from your
 *   computer if the access point has been setup in bridged mode.)
 * - Run the following command on the access point:
 * @code
 * telnet <ip address of system you noted above> 5000
 * @endcode
 * - You should see a message on the controller showing that it has accepted the connection:
 * @code
 * Accepted a TCP connection on stream_id 2
 * @endcode
 * - Type a message into the telnet session and press [ENTER]
 * - You should see the message echoed back in telnet by the controller.
 * - You can open multiple telnet sessions this way and note that they
 *   each get their own private connection and echo.
 * - You can end any telnet session by pressing ^]
 * - Every time you close a telnet session, you should see the following message on the controller:
 * @code
 * Closed TCP socket
 * @endcode
 *
 * ### Using a Python script ###
 * For stress testing the TCP echo server, we provide you a @c tcp_echo_client.py
 * file that you can run to open multiple connections to the TCP echo server on the controller
 * and rapidly send and receive data from it. This script can also open multiple TCP connections
 * in parallel if requested.
 *
 * If your AP is configured in bridge mode, then you can run this script from any computer
 * in the network. If not, you will need to copy this script to the access point and run it
 * from a terminal session in the access point.
 *
 * Once you see the following message on the controller:
 *
 * You can issue the following command from the access point (Or your PC if the access point
 * is configured for bridged mode):
 * @code
 * python tcp_echo_client.py -a <ip address of system you noted above> -c 10 -v
 * @endcode
 * This will open a connection to the TCP echo server and send and receive 10 packets.
 * The script generates random data to send and verifies the correct data was echoed
 * back by the TCP echo server.
 *
 * Run the following command to see all options you can pass to the script.
 * @code
 * python tcp_echo_client.py -h
 * @endcode
 *
 */

#include "mmhal.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmagic_controller.h"

/* Default SSID  */
#ifndef SSID
/** SSID of the AP to connect to. (Do not quote; it will be stringified.) */
#define SSID                            MorseMicro
#endif

/* Default passphrase  */
#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE                  12345678
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

/** Duration to wait for the link to be established after WLAN reports connected. */
#define LINK_STATE_TIMEOUT_MS 10000

/** Port the the TCP echo server will bind to. */
#define TCP_ECHCO_SERVER_PORT 5000

/**
 * Handler for the "Agent start" callback.
 *
 * @param controller Controller instance handle.
 * @param arg        Opaque argument that was given at the time of callback registration.
 */
void agent_start_handler(struct mmagic_controller *controller, void *arg)
{
    MM_UNUSED(controller);
    struct mmosal_semb *started = (struct mmosal_semb *)arg;
    printf("Agent start notification received\n");
    mmosal_semb_give(started);
}

/** Binary semaphore used to indicate when an agent start notification has been received. */
static struct mmosal_semb *agent_started_semb = NULL;

/**
 * This function illustrates how to establish a wlan connection using the mmagic_controller
 * interface.
 *
 * @param  controller Reference to the controller structure to use.
 *
 * @return            @c true is a wlan connection and link was established else @c false
 */
static bool wlan_connect(struct mmagic_controller *controller)
{
    printf("\n\n#### Example WLAN Connect using MMAGIC Controller ####\n\n");
    enum mmagic_status status;

    printf("Attempting to connect to %s with passphrase %s\n", STRINGIFY(SSID),
           STRINGIFY(SAE_PASSPHRASE));
    printf("This may take some time (~30 seconds)\n");
    status = mmagic_controller_set_wlan_ssid(controller, STRINGIFY(SSID));
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d setting the wlan ssid\n", status);
        return false;
    }

    mmagic_controller_set_wlan_password(controller, STRINGIFY(SAE_PASSPHRASE));
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d setting the wlan password\n", status);
        return false;
    }

    struct mmagic_core_wlan_connect_cmd_args connect_args = {
        .timeout = 120000,
    };
    status = mmagic_controller_wlan_connect(controller, &connect_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d after sending the connect command\n", status);
        return false;
    }

    struct mmagic_core_ip_status_rsp_args ip_status_rsp_args = {};
    uint32_t timeout = mmosal_get_time_ms() + LINK_STATE_TIMEOUT_MS;
    while (mmosal_time_lt(mmosal_get_time_ms(), timeout))
    {
        status = mmagic_controller_ip_status(controller, &ip_status_rsp_args);
        if ((status == MMAGIC_STATUS_OK) &&
            (ip_status_rsp_args.status.link_state == MMAGIC_IP_LINK_STATE_UP))
        {
            printf("Link Up\n");
            printf("Link is up %s. Time: %lu ms, ",
                   ip_status_rsp_args.status.dhcp_enabled ? "(DHCP)" : "(Static)",
                   mmosal_get_time_ms());
            printf("IP: %s, ", ip_status_rsp_args.status.ip_addr.addr);
            printf("Netmask: %s, ", ip_status_rsp_args.status.netmask.addr);
            printf("Gateway: %s", ip_status_rsp_args.status.gateway.addr);
            printf("\n");
            break;
        }
        mmosal_task_sleep(500);
    }

    return (status == MMAGIC_STATUS_OK) ? true : false;
}

/**
 * Arguments for beacon monitor task
 */
struct beacon_monitor_thread_args
{
    /** The controller reference */
    struct mmagic_controller *controller;
    /** The stream ID */
    uint8_t stream_id;
};

/**
 * Handler for the beacon monitor event handler callback.
 *
 * @param args The event arguments from the Agent.
 * @param arg  Opaque argument that was registered along with the handler.
 */
void beacon_monitor_rx_handler(
    const struct mmagic_wlan_beacon_rx_event_args *args, void *arg)
{
    uint32_t offset = 0;

    MM_UNUSED(arg);

    while (offset < args->vendor_ies.len)
    {
        uint8_t ie_type;
        uint8_t ie_len;
        uint32_t ii;

        if (offset + 2 > args->vendor_ies.len)
        {
            printf("Beacon IEs malformed!\n");
            break;
        }

        ie_type = args->vendor_ies.data[offset];
        ie_len = args->vendor_ies.data[offset + 1];

        offset += 2;

        if (offset + ie_len > args->vendor_ies.len)
        {
            printf("Beacon IEs malformed!\n");
            break;
        }

        printf("    IE type 0x%02x, IE len 0x%02x, Contents: ", ie_type, ie_len);
        for (ii = 0; ii < ie_len; ii++)
        {
            printf("%02x", args->vendor_ies.data[offset++]);
        }
        printf("\n");
    }
}

/**
 * This function illustrates how to subscribe to and receive custom vendor IEs from beacons.
 *
 * See @c beacon_stuffing example application for more information.
 *
 * @param controller Reference to the controller structure to use.
 */
static void beacon_monitor_example_start(struct mmagic_controller *controller)
{
    enum mmagic_status status;

    printf("\n\n#### Example Beacon Monitor using MMAGIC Controller ####\n\n");

    struct mmagic_core_wlan_beacon_monitor_enable_cmd_args beacon_monitor_args = {
        /* OUI for default Microsoft WMN/WME IE */
        .oui_filter = {.count = 1, .ouis = { { { 0x00, 0x50, 0xF2 } } }}
    };

    mmagic_controller_register_wlan_beacon_rx_handler(controller, beacon_monitor_rx_handler, NULL);

    status = mmagic_controller_wlan_beacon_monitor_enable(controller, &beacon_monitor_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d enabling beacon monitor\n", status);
        return;
    }
    printf("Enabled beacon monitor for OUI %02x:%02x:%02x\n",
           beacon_monitor_args.oui_filter.ouis[0].oui[0],
           beacon_monitor_args.oui_filter.ouis[0].oui[1],
           beacon_monitor_args.oui_filter.ouis[0].oui[2]);
}

/**
 * This function illustrates how to open a tcp client, send and receive some data and close the
 * connection. It is currently hitting a http web-page so we just expect a very basic response from
 * the http server.
 *
 * @param controller Reference to the controller structure to use.
 */
static void tcp_client_example(struct mmagic_controller *controller)
{
    printf("\n\n#### Example TCP Client using MMAGIC Controller ####\n\n");
    enum mmagic_status status;

    struct mmagic_core_tcp_connect_cmd_args tcp_connect_args = {
        .url = {.data = "morsemicro.com", .len = strlen("morsemicro.com")},
        .port = 80
    };
    struct mmagic_core_tcp_connect_rsp_args tcp_rsp_args = {};
    status = mmagic_controller_tcp_connect(controller, &tcp_connect_args, &tcp_rsp_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d establishing TCP connection\n", status);
        MMOSAL_ASSERT(false);
    }
    uint8_t tcp_stream_id = tcp_rsp_args.stream_id;
    printf("Opened TCP socket on stream_id %u\n", tcp_stream_id);

    struct mmagic_core_tcp_send_cmd_args tcp_send_cmd_args = {
        .stream_id = tcp_stream_id,
        .buffer = {.data = "GET /\n", .len = strlen("GET /\n")}
    };
    status = mmagic_controller_tcp_send(controller, &tcp_send_cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d whilst sending data\n", status);
        MMOSAL_ASSERT(false);
    }
    printf("Successfully sent GET request\n");

    struct mmagic_core_tcp_recv_cmd_args tcp_recv_cmd_args = {
        .stream_id = tcp_stream_id,
        .len = 1536,
        .timeout = 1000,
    };
    struct mmagic_core_tcp_recv_rsp_args tcp_recv_rsp_args = {};
    status = mmagic_controller_tcp_recv(controller, &tcp_recv_cmd_args, &tcp_recv_rsp_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d whilst receiving data\n", status);
    }
    else
    {
        printf("Received:\n%.*s\n", tcp_recv_rsp_args.buffer.len, tcp_recv_rsp_args.buffer.data);
    }

    struct mmagic_core_tcp_close_cmd_args tcp_close_cmd_args = {.stream_id = tcp_stream_id};
    status = mmagic_controller_tcp_close(controller, &tcp_close_cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %d whilst closing tcp socket\n", status);
        MMOSAL_ASSERT(false);
    }
    printf("Closed TCP Socket\n");
}

/**
 * Arguments for TCP echo server task
 */
struct tcp_echo_server_thread_args
{
    /** The controller reference */
    struct mmagic_controller *controller;
    /** The stream ID */
    uint8_t stream_id;
};

/**
 * This task handles a single incoming TCP connection.
 *
 * @param args The arguments for this task.
 */
static void tcp_echo_server_task(void *args)
{
    enum mmagic_status status;
    struct tcp_echo_server_thread_args *thread_args =
        (struct tcp_echo_server_thread_args *)args;
    struct mmagic_controller *controller = thread_args->controller;
    uint8_t stream_id = thread_args->stream_id;

    printf("Accepted a TCP connection on stream_id %u\n", stream_id);

    struct mmagic_core_tcp_recv_cmd_args tcp_recv_cmd_args = {
        .stream_id = stream_id,
        .len = 1536,
        .timeout = 1000,
    };
    struct mmagic_core_tcp_recv_rsp_args tcp_recv_rsp_args = {};
    struct mmagic_core_tcp_send_cmd_args tcp_send_cmd_args =
    {
        .stream_id = stream_id
    };

    while (true)
    {
        status = mmagic_controller_tcp_recv(controller, &tcp_recv_cmd_args, &tcp_recv_rsp_args);
        if (status == MMAGIC_STATUS_TIMEOUT)
        {
            continue;
        }
        else if (status == MMAGIC_STATUS_CLOSED)
        {
            printf("Connection closed by the other side!\n");
            break;
        }
        else if (status != MMAGIC_STATUS_OK)
        {
            printf("Error %d whilst receiving data\n", status);
            break;
        }
        printf("Received: %d bytes, echoing.\n", tcp_recv_rsp_args.buffer.len);

        memcpy(tcp_send_cmd_args.buffer.data, tcp_recv_rsp_args.buffer.data,
               tcp_recv_rsp_args.buffer.len);
        tcp_send_cmd_args.buffer.len = tcp_recv_rsp_args.buffer.len;
        status = mmagic_controller_tcp_send(controller, &tcp_send_cmd_args);
        if (status != MMAGIC_STATUS_OK)
        {
            printf("Error %d whilst sending data\n", status);
            break;
        }
    }

    struct mmagic_core_tcp_close_cmd_args tcp_close_cmd_args =
    {
        .stream_id = stream_id
    };
    status = mmagic_controller_tcp_close(controller, &tcp_close_cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %u whilst trying to close the TCP socket on stream_id %u\n",
               status, stream_id);
    }
    printf("Closed TCP socket\n");
}

/**
 * This function illustrates how to start a TCP server and listen for a connection. It will echo
 * back anything received on the connection once established.
 *
 * @param controller Reference to the controller structure to use.
 */
static void tcp_echo_server_start(struct mmagic_controller *controller)
{
    printf("\n\n#### Example TCP Echo Server using MMAGIC Controller ####\n\n");
    enum mmagic_status status;
    static struct tcp_echo_server_thread_args thread_args;

    struct mmagic_core_tcp_bind_cmd_args tcp_bind_cmd_args = {.port = TCP_ECHCO_SERVER_PORT};
    struct mmagic_core_tcp_bind_rsp_args tcp_bind_rsp_args = {};
    status = mmagic_controller_tcp_bind(controller, &tcp_bind_cmd_args, &tcp_bind_rsp_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %u whilst opening the tcp socket\n", status);
        MMOSAL_ASSERT(false);
    }
    uint8_t tcp_socket_stream_id = tcp_bind_rsp_args.stream_id;
    printf("Opened listening socket (Port %u) on stream_id %u\n",
           tcp_bind_cmd_args.port, tcp_socket_stream_id);

    struct mmagic_core_tcp_accept_cmd_args tcp_accept_cmd_args = {
        .stream_id = tcp_socket_stream_id
    };
    struct mmagic_core_tcp_accept_rsp_args tcp_accept_rsp_args = {};
    while (true)
    {
        status = mmagic_controller_tcp_accept(controller,
                                              &tcp_accept_cmd_args,
                                              &tcp_accept_rsp_args);
        if (status != MMAGIC_STATUS_OK)
        {
            printf("Error %u whilst trying to accept a TCP connetion\n", status);
            break;
        }

        thread_args.stream_id = tcp_accept_rsp_args.stream_id;
        thread_args.controller = controller;

        mmosal_task_create(tcp_echo_server_task, &thread_args,
                        MMOSAL_TASK_PRI_LOW, 2048, "tcp_echo_server_task");
    }

    struct mmagic_core_tcp_close_cmd_args tcp_close_cmd_args =
    {
        .stream_id = tcp_socket_stream_id
    };
    status = mmagic_controller_tcp_close(controller, &tcp_close_cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        printf("Error %u whilst trying to close the listening socket on stream_id %u\n",
               status, tcp_socket_stream_id);
    }
    printf("Closed listening socket\n");
}

/**
 * Runs the examples.
 *
 * @param args Not used.
 */
static void run_examples_task(void *args)
{
    MM_UNUSED(args);

    agent_started_semb = mmosal_semb_create("agent_started");
    struct mmagic_controller_init_args init_args = MMAGIC_CONTROLLER_ARGS_INIT;
    init_args.agent_start_cb = agent_start_handler;
    init_args.agent_start_arg = (void *)agent_started_semb;

    struct mmagic_controller *controller = mmagic_controller_init(&init_args);

    printf("M2M Controller enabled. Awaiting Agent start\n");
    mmosal_semb_wait(agent_started_semb, UINT32_MAX);

    if (!wlan_connect(controller))
    {
        printf("Failed to connect\n");
        return;
    }
    printf("WLAN connection established\n");

    beacon_monitor_example_start(controller);

    tcp_client_example(controller);

    tcp_echo_server_start(controller);
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nM2M Controller Example (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Keep controller awake for now */
    mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_APP_MIN);

    /* Run the examples in a different task to cater for the higher stack size requirements */
    mmosal_task_create(run_examples_task, NULL,
                       MMOSAL_TASK_PRI_LOW, 2048, "run_examples_task");
}
