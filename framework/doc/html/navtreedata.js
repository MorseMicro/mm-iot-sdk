/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Morse Micro IoT SDK", "index.html", [
    [ "Introduction", "index.html", [
      [ "Morselib Overview", "index.html#autotoc_md54", null ]
    ] ],
    [ "Morse Micro IoT Reference Platforms", "MMPLATFORMS.html", "MMPLATFORMS" ],
    [ "Getting Started", "GETTING_STARTED.html", [
      [ "Prerequisites", "GETTING_STARTED.html#autotoc_md57", null ],
      [ "Unpacking the software package", "GETTING_STARTED.html#UNPACKING_THE_SW", null ],
      [ "Development PC setup", "GETTING_STARTED.html#GETTING_STARTED_DEV_PC_SETUP", [
        [ "Dependencies", "GETTING_STARTED.html#autotoc_md58", null ],
        [ "Python environment", "GETTING_STARTED.html#autotoc_md59", null ]
      ] ],
      [ "Configure Application", "GETTING_STARTED.html#autotoc_md60", [
        [ "Launching OpenOCD", "GETTING_STARTED.html#GETTING_STARTED_OPENOCD", null ],
        [ "Setting Config Store", "GETTING_STARTED.html#SET_APP_CONFIGURATION", null ]
      ] ],
      [ "Building Firmware", "GETTING_STARTED.html#BUILDING_FIRMWARE", null ],
      [ "Programming", "GETTING_STARTED.html#PROGRAMMING_FIRMWARE", [
        [ "Launching Miniterm", "GETTING_STARTED.html#autotoc_md61", null ],
        [ "Launching Application", "GETTING_STARTED.html#autotoc_md62", null ]
      ] ],
      [ "Troubleshooting", "GETTING_STARTED.html#autotoc_md63", [
        [ "Unable to find BCF file entry in config store", "GETTING_STARTED.html#BCF_CONFIG_STORE", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md64", null ],
          [ "Possible cause", "GETTING_STARTED.html#autotoc_md65", null ]
        ] ],
        [ "Unable to connect to the MCU with OpenOCD", "GETTING_STARTED.html#autotoc_md66", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md67", null ],
          [ "Possible cause 1", "GETTING_STARTED.html#autotoc_md68", null ],
          [ "Possible cause 2", "GETTING_STARTED.html#autotoc_md69", null ]
        ] ]
      ] ],
      [ "Additional steps for mm-ekh08-wb55 and mm-ekh18-wb55 platforms", "GETTING_STARTED.html#WB55_EXTRA_STEPS", null ]
    ] ],
    [ "SDK Overview", "OVERVIEW.html", [
      [ "Application Configuration", "OVERVIEW.html#APP_CONFIGURATION", [
        [ "Package Structure", "OVERVIEW.html#autotoc_md70", null ],
        [ "Example Application Structure", "OVERVIEW.html#autotoc_md71", null ],
        [ "Python environment", "OVERVIEW.html#autotoc_md72", null ],
        [ "Board Configuration File", "OVERVIEW.html#autotoc_md73", null ]
      ] ]
    ] ],
    [ "Debugging with GDB", "DEBUGGING.html", null ],
    [ "Host Power Save", "MMHOSTPOWERSAVE.html", [
      [ "Overview", "MMHOSTPOWERSAVE.html#autotoc_md76", null ],
      [ "Implementation", "MMHOSTPOWERSAVE.html#autotoc_md77", null ],
      [ "Deep sleep decision logic", "MMHOSTPOWERSAVE.html#autotoc_md78", null ],
      [ "Deep sleep exceptions", "MMHOSTPOWERSAVE.html#autotoc_md79", null ],
      [ "Debugging during deep sleep", "MMHOSTPOWERSAVE.html#MMHOSTPOWERSAVE_DEBUG_DEEP_SLEEP", null ]
    ] ],
    [ "Morse Micro CLI API", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html", [
      [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md80", null ],
      [ "Module wlan: Wireless LAN management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md81", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md82", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md83", [
          [ "wlan-connect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md84", null ],
          [ "wlan-disconnect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md85", null ],
          [ "wlan-scan", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md86", null ],
          [ "wlan-get_rssi", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md87", null ],
          [ "wlan-get_mac_addr", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md88", null ],
          [ "wlan-wnm_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md89", null ],
          [ "wlan-beacon_monitor_enable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md90", null ],
          [ "wlan-beacon_monitor_disable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md91", null ],
          [ "wlan-standby_enter", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md92", null ],
          [ "wlan-standby_exit", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md93", null ],
          [ "wlan-standby_set_status_payload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md94", null ],
          [ "wlan-standby_set_wake_filter", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md95", null ],
          [ "wlan-standby_set_config", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md96", null ],
          [ "wlan-get_sta_status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md97", null ]
        ] ]
      ] ],
      [ "Module ip: IP Stack Management", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md98", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md99", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md100", [
          [ "ip-status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md101", null ],
          [ "ip-reload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md102", null ],
          [ "ip-enable_tcp_keepalive_offload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md103", null ],
          [ "ip-disable_tcp_keepalive_offload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md104", null ],
          [ "ip-set_whitelist_filter", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md105", null ],
          [ "ip-clear_whitelist_filter", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md106", null ]
        ] ]
      ] ],
      [ "Module ping: Ping application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md107", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md108", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md109", [
          [ "ping-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md110", null ]
        ] ]
      ] ],
      [ "Module iperf: Iperf application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md111", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md112", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md113", [
          [ "iperf-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md114", null ]
        ] ]
      ] ],
      [ "Module sys: System management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md115", [
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md116", [
          [ "sys-reset", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md117", null ],
          [ "sys-deep_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md118", null ],
          [ "sys-get_version", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md119", null ]
        ] ]
      ] ],
      [ "Enum definitions", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md120", [
        [ "Security type", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_security_type", null ],
        [ "Pmf mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_pmf_mode", null ],
        [ "Power save mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_power_save_mode", null ],
        [ "Mcs10 mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_mcs10_mode", null ],
        [ "Duty cycle mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_duty_cycle_mode", null ],
        [ "Station type", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_station_type", null ],
        [ "Status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_status", null ],
        [ "Iperf mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_iperf_mode", null ],
        [ "Iperf state", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_iperf_state", null ],
        [ "Ip link state", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_ip_link_state", null ],
        [ "Deep sleep mode", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_deep_sleep_mode", null ],
        [ "Standby mode exit reason", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_standby_mode_exit_reason", null ],
        [ "Sta state", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_sta_state", null ],
        [ "Sta event", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_sta_event", null ]
      ] ]
    ] ],
    [ "Example Applications", "MMAPPS.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Modules", "modules.html", "modules" ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", "functions_vars" ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"DEBUGGING.html",
"dir_c0e438e5c91396ac2e47b25ca41124e5.html",
"group__MMAGIC__CONTROLLER__DATA__TYPES.html#ggadc12f63888a42e66b656dd64b32b37e9aa527b10cd3723f481594656a613f7128",
"group__MMAGIC__CONTROLLER__WLAN.html",
"group__MMHAL__WLAN__PKT.html#ga3be752e89ee0004b7631183bf47469a2",
"group__MMOSAL__TASKS.html#ga23471004fbe03bda76cbebae3be98baa",
"group__MMWLAN__SCAN.html",
"md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md118",
"structmmagic__core__mqtt__subscribe__cmd__args.html",
"structmmwlan__beacon__vendor__ie__filter.html#a94a431832e58ae2dca83f0b90f5e589c",
"structstruct__ip__status.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';