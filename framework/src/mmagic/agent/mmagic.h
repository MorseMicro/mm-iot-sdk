/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "core/autogen/mmagic_core_types.h"
#include "mmwlan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup MMAGIC MMAGIC: CLI and Machine-to-Machine interface infrastructure
 *
 * @section MMAGIC_OVERVIEW Overview
 *
 * MMAGIC is infrastructure for creating binary and command line interface (CLI) APIs for either
 * Machine-to-Machine (M2M) or user interactive operation of a device. The CLI and M2M interfaces
 * provide the same core functionality.
 *
 * The goal of the CLI is to provide a user friendly command line interface with an appropriately
 * designed command set for interactive use. It is intended to be user friendly with tab completion
 * of commands, command history, and a help menu.
 *
 * The M2M interface is designed to be powerful and efficient with a fully-featured command set
 * and an efficient communications protocol. It is designed to support multiple transports (e.g.,
 * UART and SPI).
 *
 *
 * @section MMAGIC_M2M Machine-to-Machine (M2M) Interface
 *
 * The M2M interface supports a three chip solution, as shown in the diagram below. The
 * __Controller__ is where application-specific code runs and it communicates with the Agent
 * via the M2M interface (e.g., over SPI or UART). The __Agent__ communicates with the Morse
 * transceiver and implements the WLAN drivers and UMAC as well as the network stack. It presents
 * a control API to the Controller via the M2M interface.
 *
 * On the Agent side, the software stack is built using the MM-IoT-SDK. On the Controller side,
 * there is no dependency on the MM-IoT-SDK, but some sample code has been provided to simplify
 * implementation of the Controller. For more details see the example applications below, as
 * well as the @ref MMAGIC_AGENT and @ref MMAGIC_CONTROLLER.
 *
 * @image html mmagic-3chip.png "Machine-to-Machine (M2M) interface for three chip solution"
 * @image latex mmagic-3chip.png "Machine-to-Machine (M2M) interface for three chip solution"
 *
 * @section MMAGIC_EXAMPLES Examples
 *
 * Three example applications are provided for MMAGIC:
 *
 * * Command line interface and UART-based M2M Controller (@ref cli.c)
 * * SPI-based M2M Agent for @ref mm-ekh08-u575 platform (@c m2m_agent.c)
 * * SPI-based M2M Controller for @ref mm-ekh08-u575 platform (@c m2m_controller.c)
 */

/**
 * @ingroup  MMAGIC
 * @defgroup MMAGIC_M2M_PROTOCOL Morse M2M Protocol
 * @{
 *
 * This section describes the binary protocol used by the Machine-to-Machine Interface (M2M)
 * for communication between an Agent and a Controller.
 *
 * The diagram below shows the roles of the Agent and Controller. The Agent connects to the
 * Morse transceiver and implements the drivers and network stack. The Controller runs application
 * code and controls the Agent via the M2M protocol.
 *
 * @image html mmagic-3chip.png  "Machine-to-Machine (M2M) interface for three chip solution"
 * @image latex mmagic-3chip.png  "Machine-to-Machine (M2M) interface for three chip solution"
 *
 * @section MMAGIC_M2M_PROTOCOL_STACK Protocol Stack
 *
 * The following diagram shows the M2M interface protocol software stack. At the top of the
 * stack is the application code that runs on the Controller. This interacts with the Controller
 * library via the @ref MMAGIC_CONTROLLER, which is defined in `mmagic_controller.h`. The
 * Controller library takes care of serializing and deserializing data for communication with
 * the Agent. It implements the controller side of the "Link Layer Control (LLC)" protocol that
 * is responsible for multiplexing and synchronizing commands, responses, and events.
 *
 * Actual communication between the Controller and the Agent is the responsibility of the
 * data link layer. This layer provides a semi-reliable transport for communicating packets
 * asychronously between the Controller and Agent. Because the data link implementation is
 * platform and hardware dependent it needs to be implemented on a per-platform basis.
 * Protocols are currently defined for SPI and UART transports (see
 * @ref MMAGIC_M2M_PROTOCOL_SPI_DATALINK and @ref MMAGIC_M2M_PROTOCOL_UART_DATALINK respectively).
 * On the Controller side, the @ref MMAGIC_DATALINK_CONTROLLER provides the interface
 * between the Controller library and the data link implementation.
 *
 * @image html mmagic-m2m-stack.drawio.png  "Machine-to-Machine (M2M) interface protocol stack"
 * @image latex mmagic-m2m-stack.drawio.png  "Machine-to-Machine (M2M) interface protocol stack"
 *
 * @section MMAGIC_M2M_PROTOCOL_SPI_DATALINK M2M SPI Data Link Protocol
 *
 * The M2M SPI Data Link protocol is asymmetric. The Controller acts as SPI master and the
 * Agent acts as SPI slave. The protocol uses 6 pins for communication:
 *
 * | Pin Name | Direction         | Description                    |
 * |----------|-------------------|--------------------------------|
 * | SPI MISO | Agent->Controller | SPI data: master in, slave out |
 * | SPI MOSI | Controller->Agent | SPI data: master out, slave in |
 * | SPI CLK  | Controller->Agent | SPI clock                      |
 * | SPI CS   | Controller->Agent | SPI chip select                |
 * | Wake     | Controller->Agent | Agent wake up                  |
 * | Ready    | Agent->Controller | Agent ready/IRQ                |
 *
 * Transfers on the bus are initiated by the Controller. Two types of transfer are currently
 * defined: packet writes and packet reads. Packet writes transfer a packet of data from
 * Controller to Agent while packet reads transfer a packet of data from Agent to Controller.
 *
 * Each transfer is made up of multiple SPI transactions. For each SPI transaction, the
 * Controller first asserts the SPI CS pin, then transfers the data followed by a 16 bit CRC,
 * then deasserts the CS pin. Each SPI transaction is half duplex (either a read or a write).
 *
 * @subsection MMAGIC_M2M_PROTOCOL_SPI_DATALINK_WRITE SPI packet write transfers
 *
 * A packet write transfer proceeds as follows:
 *
 * -# The Controller asserts the Wake pin by driving it high.
 * -# The Controller polls the Ready pin until the Agent drives it high.
 * -# The Controller performs a SPI write transaction to write the Transfer Header
 *    (see @ref MMAGIC_M2M_PROTOCOL_SPI_DATALINK_STRUCTS) with Transfer Type set to WRITE and
 *    Length set to the length of the payload.
 * -# The Controller polls the Ready pin until the Agent drives it low.
 * -# The Controller performs a SPI write transaction to write the data packet.
 * -# The Controller polls the Ready pin until the Agent drives it high.
 * -# The Controller performs a SPI read transaction to read the Ack Trailer
 *    (see @ref MMAGIC_M2M_PROTOCOL_SPI_DATALINK_STRUCTS).
 * -# The Controller deasserts the Wake pin by driving it low.
 *
 * @include{doc} agent/m2m_datalink/mmagic_datlaink_spi_write.puml
 *
 * @subsection MMAGIC_M2M_PROTOCOL_SPI_DATALINK_READ SPI packet read transfers
 *
 * A packet read transfer proceeds as follows:
 *
 * A read transfer proceeds as follows:
 *
 * -# The Controller asserts the Wake pin by driving it high.
 * -# The Controller polls the Ready pin until the Agent drives it high.
 * -# The Controller performs a SPI write transaction to write the Transfer Header
 *    (see @ref MMAGIC_M2M_PROTOCOL_SPI_DATALINK_STRUCTS) with Transfer Type set to READ and
 *    Length set to zero.
 * -# The Controller polls the Ready pin until the Agent drives it low.
 * -# The Controller performs a SPI read transaction to read the length of the packet.
 * -# The Controller polls the Ready pin until the Agent drives it high.
 * -# The Controller performs a SPI read transaction to read the packet.
 * -# The Controller deasserts the Wake pin by driving it low.
 *
 * @include{doc} agent/m2m_datalink/mmagic_datlaink_spi_read.puml
 *
 * @subsection MMAGIC_M2M_PROTOCOL_SPI_DATALINK_IRQ Interrupt requests
 *
 * The SPI protocol is asymmetric and the Controller acts as bus master. Thus the Agent needs
 * a means of notifying the Controller that it has a packet ready for it to read. This is
 * done by asserting the Ready pin (driving it high) while the Wake pin is low. If the
 * Controller detects a high level on the Ready pin while it is not asserting the Wake pin
 * then it knows that it needs to perform a packet read transfer.
 *
 * If the Controller performs a packet read transfer when the Agent does not have a packet
 * buffered for read then the Agent will return a length of zero.
 *
 * @subsection MMAGIC_M2M_PROTOCOL_SPI_DATALINK_STRUCTS Data structures
 *
 * The `Transfer Header` is written by the Controller at the start of each transfer and comprises
 * a one octet Transfer Type field followed by a two octet Length field. The use of these fields
 * is defined in the sections above.
 *
 * The `Ack Trailer` is read by the Controller at the end of a packet write transfer. It comprises
 * a single octet indicating positive acknowledgement (CRC was valid) or negative
 * acknowledgement (CRC or other failure).
 *
 * The `Length` field is read during a packet read transfer and is a two octet field.
 *
 * All multi-octet fields are little endian except the CRC, which is big endian.
 *
 * @section MMAGIC_M2M_PROTOCOL_UART_DATALINK M2M UART Data Link Protocol
 *
 * The UART-based data link protocol is asynchronous, meaning that packets can be transmitted
 * and received at any time. This means that the protocol is symmetrical.
 *
 * When either side has a packet queued for transmission the following protocol is observed:
 *
 * -# Optionally, a CRC is calculated over the packet payload (CRC16 XModem)
 * -# A header octet is prepended to the packet. The current implementation of the protocol
 *    uses the most significant bit to indicate whether a CRC is present (1 = present, 0 = absent).
 *    The remainder of the bits are reserved and must be set to zero.
 * -# The packet is encoded using the SLIP encoding and is transmitted via the UART.
 *
 * Packet reception mirrors transmission.
 *
 * For an example implementation see `mmagic/agent/m2m_datalink/mmagic_datalink_uart.c`. This
 * data link can be enabled when using the @ref cli.c examaple application by setting the
 * @ref MMCONFIG variable `cli.mode` to `m2m`.
 *
 * @section MMAGIC_M2M_PROTOCOL_PACKETS M2M Packet Structure
 *
 * The following diagram shows the structure of M2M packets, excluding data link layer framing.
 *
 * @image html mmagic-m2m-packet.drawio.png  "Machine-to-Machine (M2M) interface packet structure"
 * @image latex mmagic-m2m-packet.drawio.png  "Machine-to-Machine (M2M) interface packet structure"
 *
 * Since both sides of the protocol are implemented by the MMAGIC Agent and Controller libraries,
 * details of these protocol layers are not documented here.
 *
 * @}
 */

/**
 * @ingroup  MMAGIC
 * @defgroup MMAGIC_AGENT_COMMON Morse M2M Agent/CLI Common API
 * @{
 *
 * This API is common use by both the MMAGIC M2M Agent and CLI.
 *
 */

/** The stream ID of the control stream */
#define CONTROL_STREAM  0

/** Maximum length of the @c app_version string (excluding null terminator). */
#define MMAGIC_SYS_MAX_APP_VERSION_LENGTH   (31)

/**
 * Typedef for callback function that is invoked by MMAGIC to request a change in deep sleep
 * mode.
 *
 * @param mode   The requested mode.
 * @param cb_arg Opaque argument that was provided when the callback was registered.
 *
 * @returns true if the mode was set successfully; false on failure (e.g., unsupported mode).
 */
typedef bool (*mmagic_set_deep_sleep_mode_cb_t)(enum mmagic_deep_sleep_mode mode, void *cb_arg);

/** @} */

/**
 * @ingroup  MMAGIC
 * @defgroup MMAGIC_CLI Morse Command Line Interface (CLI)
 * @{
 */

/** CLI initialization arguments structure. */
struct mmagic_cli_init_args
{
    /** Application version string. This is a null terminated string. */
    char app_version[MMAGIC_SYS_MAX_APP_VERSION_LENGTH + 1];
    /** Opaque arg to pass to @c tx_cb. */
    void *tx_cb_arg;
    /** Transmit callback. */
    void (*tx_cb)(const char *buf, size_t len, void *cb_arg);
    /** Opaque arg to pass to @c set_deep_sleep_mode_cb. */
    void *set_deep_sleep_mode_cb_arg;
    /** Set deep sleep mode callback */
    mmagic_set_deep_sleep_mode_cb_t set_deep_sleep_mode_cb;
    /** Reference to the regulatory database, used to initialize wlan */
    const struct mmwlan_regulatory_db *reg_db;
};

/**
 * Initialize and enable the CLI.
 *
 * @param args Initialization arguments structure.
 *
 * @returns     A handle to the CLI instance. @c NULL on error.
 */
struct mmagic_cli *mmagic_cli_init(const struct mmagic_cli_init_args *args);

/**
 * This function is used to pass received characters to the MMAGIC CLI.
 *
 * @param ctx CLI instance handle (as returned by @ref mmagic_cli_init).
 * @param buf Buffer containing received data.
 * @param len Length of the received data.
 */
void mmagic_cli_rx(struct mmagic_cli *ctx, const char *buf, size_t len);

/** @} */

/**
 * @ingroup  MMAGIC
 * @defgroup MMAGIC_AGENT Morse M2M Agent
 * @{
 */

/** M2M initialization args */
struct mmagic_m2m_agent_init_args
{
    /** Application version string. This is a null terminated string. */
    char app_version[MMAGIC_SYS_MAX_APP_VERSION_LENGTH + 1];
    /** Reference to the regulatory database, used to initialize wlan */
    const struct mmwlan_regulatory_db *reg_db;
};

/**
 * Initialize the M2M agent.
 *
 * @note this may block whilst the controller is notified that the agent has started.
 *
 * @param  args Initialization arguments.
 *
 * @return      A handle to the newly created Agent. NULL on error.
 */
struct mmagic_m2m_agent *mmagic_m2m_agent_init(const struct mmagic_m2m_agent_init_args *args);

/** @} */

#ifdef __cplusplus
}
#endif
