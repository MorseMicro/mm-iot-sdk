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
      [ "Morselib Overview", "index.html#autotoc_md46", [
        [ "API production readiness annotations", "index.html#autotoc_md47", null ]
      ] ]
    ] ],
    [ "Morse Micro IoT Reference Platforms", "MMPLATFORMS.html", "MMPLATFORMS" ],
    [ "Getting Started", "GETTING_STARTED.html", [
      [ "Prerequisites", "GETTING_STARTED.html#autotoc_md50", null ],
      [ "Unpacking the software package", "GETTING_STARTED.html#UNPACKING_THE_SW", null ],
      [ "Development PC setup", "GETTING_STARTED.html#GETTING_STARTED_DEV_PC_SETUP", [
        [ "Dependencies", "GETTING_STARTED.html#autotoc_md51", null ],
        [ "Python environment", "GETTING_STARTED.html#autotoc_md52", null ]
      ] ],
      [ "Configure Application", "GETTING_STARTED.html#autotoc_md53", [
        [ "Launching OpenOCD", "GETTING_STARTED.html#GETTING_STARTED_OPENOCD", null ],
        [ "Setting Config Store", "GETTING_STARTED.html#SET_APP_CONFIGURATION", null ]
      ] ],
      [ "Building Firmware", "GETTING_STARTED.html#BUILDING_FIRMWARE", null ],
      [ "Programming", "GETTING_STARTED.html#PROGRAMMING_FIRMWARE", [
        [ "Launching Miniterm", "GETTING_STARTED.html#autotoc_md54", null ],
        [ "Launching Application", "GETTING_STARTED.html#autotoc_md55", null ]
      ] ],
      [ "Troubleshooting", "GETTING_STARTED.html#autotoc_md56", [
        [ "Unable to find BCF file entry in config store", "GETTING_STARTED.html#BCF_CONFIG_STORE", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md57", null ],
          [ "Possible cause", "GETTING_STARTED.html#autotoc_md58", null ]
        ] ],
        [ "Unable to connect to the MCU with OpenOCD", "GETTING_STARTED.html#autotoc_md59", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md60", null ],
          [ "Possible cause 1", "GETTING_STARTED.html#autotoc_md61", null ],
          [ "Possible cause 2", "GETTING_STARTED.html#autotoc_md62", null ]
        ] ]
      ] ],
      [ "Additional steps for mm-ekh08-wb55 and mm-ekh18-wb55 platforms", "GETTING_STARTED.html#WB55_EXTRA_STEPS", null ]
    ] ],
    [ "SDK Overview", "OVERVIEW.html", [
      [ "Application Configuration", "OVERVIEW.html#APP_CONFIGURATION", [
        [ "Package Structure", "OVERVIEW.html#autotoc_md63", null ],
        [ "Example Application Structure", "OVERVIEW.html#autotoc_md64", null ],
        [ "Python environment", "OVERVIEW.html#autotoc_md65", null ],
        [ "Board Configuration File", "OVERVIEW.html#ABOUT_BCF", null ]
      ] ]
    ] ],
    [ "Debugging with GDB", "DEBUGGING.html", null ],
    [ "Host Power Save", "MMHOSTPOWERSAVE.html", [
      [ "Overview", "MMHOSTPOWERSAVE.html#autotoc_md68", null ],
      [ "Implementation", "MMHOSTPOWERSAVE.html#autotoc_md69", null ],
      [ "Deep sleep decision logic", "MMHOSTPOWERSAVE.html#autotoc_md70", null ],
      [ "Deep sleep exceptions", "MMHOSTPOWERSAVE.html#autotoc_md71", null ],
      [ "Debugging during deep sleep", "MMHOSTPOWERSAVE.html#MMHOSTPOWERSAVE_DEBUG_DEEP_SLEEP", null ]
    ] ],
    [ "LWIP on-demand timers", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_res_doc_on_demand_timers.html", [
      [ "How it's implemented", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_res_doc_on_demand_timers.html#autotoc_md73", null ],
      [ "When to enable or disable it", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_res_doc_on_demand_timers.html#autotoc_md74", null ],
      [ "Troubleshooting", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_res_doc_on_demand_timers.html#autotoc_md75", null ]
    ] ],
    [ "BCF regulatory channel support", "BCF_REGDOM_SUPPORT.html", [
      [ "Supported regulatory domains", "BCF_REGDOM_SUPPORT.html#autotoc_md76", null ],
      [ "AU", "BCF_REGDOM_SUPPORT.html#autotoc_md77", [
        [ "AU 802.11 2020", "BCF_REGDOM_SUPPORT.html#autotoc_md78", null ],
        [ "AU 802.11 2024", "BCF_REGDOM_SUPPORT.html#autotoc_md79", null ],
        [ "AU 802.11 revMF", "BCF_REGDOM_SUPPORT.html#autotoc_md80", null ]
      ] ],
      [ "CA", "BCF_REGDOM_SUPPORT.html#autotoc_md81", null ],
      [ "EU", "BCF_REGDOM_SUPPORT.html#autotoc_md82", null ],
      [ "GB", "BCF_REGDOM_SUPPORT.html#autotoc_md83", null ],
      [ "IN", "BCF_REGDOM_SUPPORT.html#autotoc_md84", null ],
      [ "JP", "BCF_REGDOM_SUPPORT.html#autotoc_md85", null ],
      [ "KR", "BCF_REGDOM_SUPPORT.html#autotoc_md86", null ],
      [ "NZ", "BCF_REGDOM_SUPPORT.html#autotoc_md87", null ],
      [ "US", "BCF_REGDOM_SUPPORT.html#autotoc_md88", null ]
    ] ],
    [ "Morse Micro CLI API", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html", [
      [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md89", null ],
      [ "Module wlan: Wireless LAN management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md90", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md91", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md92", [
          [ "wlan-connect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md93", null ],
          [ "wlan-disconnect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md94", null ],
          [ "wlan-scan", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md95", null ],
          [ "wlan-get_rssi", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md96", null ],
          [ "wlan-get_mac_addr", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md97", null ],
          [ "wlan-wnm_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md98", null ],
          [ "wlan-beacon_monitor_enable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md99", null ],
          [ "wlan-beacon_monitor_disable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md100", null ],
          [ "wlan-get_sta_status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md101", null ],
          [ "wlan-dpp_push_button_start", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md102", null ],
          [ "wlan-dpp_stop", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md103", null ]
        ] ]
      ] ],
      [ "Module ip: IP Stack Management", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md104", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md105", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md106", [
          [ "ip-status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md107", null ],
          [ "ip-reload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md108", null ]
        ] ]
      ] ],
      [ "Module ping: Ping application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md109", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md110", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md111", [
          [ "ping-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md112", null ]
        ] ]
      ] ],
      [ "Module iperf: Iperf application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md113", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md114", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md115", [
          [ "iperf-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md116", null ]
        ] ]
      ] ],
      [ "Module sys: System management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md117", [
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md118", [
          [ "sys-reset", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md119", null ],
          [ "sys-deep_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md120", null ],
          [ "sys-get_version", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md121", null ],
          [ "sys-get_stats", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md122", null ]
        ] ]
      ] ],
      [ "Enum definitions", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md123", [
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
        [ "Sta state", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_sta_state", null ],
        [ "Sta event", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_sta_event", null ],
        [ "Socket proto", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_socket_proto", null ],
        [ "Subsystem id", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_subsystem_id", null ]
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
"BCF_REGDOM_SUPPORT.html",
"doc_8h_source.html",
"group__MMAGIC__CONTROLLER__INTERNAL.html#gga62fb515a7270a1a2eec77f80cd4fb9b7aa73d6626894c95bc471b064d7203322a",
"group__MMBUF.html#gabce9aabae051540b3629557fcded1be3",
"group__MMIPAL.html#ggafc9d0260b07ca329e4fc2393a769c3cda7af3e1dc0c8e2336742f2852c0a799c7",
"group__MMUTILS.html#ga2b4b5b12d04302a9ada0dee5a0342051",
"group__MMWLAN__SCAN.html#gga099b3d1bfda216a73f6412101207e9d6abec4a041ebb210f38e1d648afa076426",
"mm__app__loadconfig_8h.html#a181fc13c7551d2a68f81aaebb0d9711f",
"structmm__rsn__information.html#a48470713afc679e6de9c08043fbdc373",
"structmmpkt.html#a58f6ac947f3c1a2b22890a34c1143fbb",
"structmmwlan__version.html#af6023405bb8e6cedaf679b3885d8b423"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';