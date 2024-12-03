#
# Copyright 2024 Morse Micro
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

MMAGIC_AGENT_SRCS_C += core/autogen/mmagic_core_data.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_types.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_wlan.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_ip.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_ping.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_iperf.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_sys.c
MMAGIC_AGENT_SRCS_C += core/mmagic_core_tcp.c

MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_internal.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_wlan.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_wlan.def
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ip.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ip.def
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ping.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_ping.def
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_iperf.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_iperf.def
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_sys.h
MMAGIC_AGENT_SRCS_H += cli/autogen/mmagic_cli_sys.def

MMAGIC_AGENT_SRCS_C += cli/mmagic_cli.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_internal.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_wlan.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_ip.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_ping.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_iperf.c
MMAGIC_AGENT_SRCS_C += cli/mmagic_cli_sys.c

MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_internal.h
MMAGIC_AGENT_SRCS_H += m2m_api/mmagic_m2m_agent.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_wlan.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_ip.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_ping.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_iperf.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_sys.h
MMAGIC_AGENT_SRCS_H += m2m_api/autogen/mmagic_m2m_tcp.h

MMAGIC_AGENT_SRCS_C += m2m_api/mmagic_m2m_agent.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_wlan.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_ip.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_ping.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_iperf.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_sys.c
MMAGIC_AGENT_SRCS_C += m2m_api/autogen/mmagic_m2m_tcp.c

MMAGIC_AGENT_SRCS_H += m2m_llc/mmagic_llc_agent.h
MMAGIC_AGENT_SRCS_C += m2m_llc/mmagic_llc_agent.c

MMAGIC_AGENT_SRCS_H += mmagic_datalink_agent.h

MMAGIC_AGENT_SRCS_C += m2m_datalink/mmagic_datalink_uart.c

MMIOT_SRCS_C += $(addprefix $(MMAGIC_AGENT_DIR)/,$(MMAGIC_AGENT_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMAGIC_AGENT_DIR)/,$(MMAGIC_AGENT_SRCS_H))
