#
# Copyright 2025 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MMAGIC_AGENT_DIR = src/mmagic/agent

MMIOT_INCLUDES += $(MMAGIC_AGENT_DIR)

MMAGIC_AGENT_SRCS_H += mmagic.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_data.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_types.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_wlan.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_ip.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_ping.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_iperf.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_sys.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_tcp.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_tls.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_ntp.h
MMAGIC_AGENT_SRCS_H += core/autogen/mmagic_core_mqtt.h

MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_data.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_types.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_utils.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_wlan.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_wlan.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_ip.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_ip.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_ping.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_ping.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_iperf.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_iperf.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_sys.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_sys.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_tcp.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_tcp.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_tls.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_tls.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_ntp.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_ntp.c
MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_autogen_mqtt.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_mqtt.c

MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_internal.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_wlan.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ip.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ping.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_iperf.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_sys.h

MMAGIC_AGENT_SRCS_C += cli/mmagic_cli.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_internal.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen_wlan.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_wlan.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen_ip.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_ip.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen_ping.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_ping.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen_iperf.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_iperf.c
MMAGIC_AGENT_SRCS_C += cli/autogen/mmagic_cli_autogen_sys.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_sys.c

MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_internal.h
MMAGIC_AGENT_SRCS_H += m2m_api/mmagic_m2m_agent.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_wlan.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_ip.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_ping.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_iperf.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_sys.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_tcp.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_tls.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_ntp.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_mqtt.h

MMAGIC_AGENT_SRCS_C += m2m_api/mmagic_m2m_agent.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_wlan.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_ip.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_ping.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_iperf.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_sys.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_tcp.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_tls.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_ntp.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_autogen_mqtt.c

MMAGIC_AGENT_SRCS_H += m2m_llc/mmagic_llc_agent.h
MMAGIC_AGENT_SRCS_C += m2m_llc/mmagic_llc_agent.c

MMAGIC_AGENT_SRCS_H += mmagic_datalink_agent.h

MMAGIC_AGENT_SRCS_C += m2m_datalink/mmagic_datalink_uart.c

MMIOT_SRCS_C += $(addprefix $(MMAGIC_AGENT_DIR)/,$(MMAGIC_AGENT_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMAGIC_AGENT_DIR)/,$(MMAGIC_AGENT_SRCS_H))
