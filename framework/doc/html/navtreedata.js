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
      [ "System Requirements", "index.html#autotoc_md54", [
        [ "Core architecture", "index.html#autotoc_md55", null ],
        [ "RAM and flash", "index.html#autotoc_md56", null ]
      ] ]
    ] ],
    [ "Morse Micro IoT Reference Platforms", "MMPLATFORMS.html", "MMPLATFORMS" ],
    [ "Getting Started", "GETTING_STARTED.html", [
      [ "Prerequisites", "GETTING_STARTED.html#autotoc_md58", null ],
      [ "Unpacking the software package", "GETTING_STARTED.html#UNPACKING_THE_SW", null ],
      [ "Package contents", "GETTING_STARTED.html#autotoc_md59", null ],
      [ "Development PC setup", "GETTING_STARTED.html#GETTING_STARTED_DEV_PC_SETUP", [
        [ "Python environment", "GETTING_STARTED.html#autotoc_md60", null ]
      ] ],
      [ "AP configuration", "GETTING_STARTED.html#autotoc_md61", null ],
      [ "Application overview", "GETTING_STARTED.html#autotoc_md62", [
        [ "Ping Example Application Structure", "GETTING_STARTED.html#autotoc_md63", null ],
        [ "Application Configuration", "GETTING_STARTED.html#APP_CONFIGURATION", [
          [ "MMConfig Application configuration", "GETTING_STARTED.html#autotoc_md64", null ],
          [ "Legacy Application Configuration", "GETTING_STARTED.html#autotoc_md65", null ]
        ] ]
      ] ],
      [ "Building Firmware", "GETTING_STARTED.html#BUILDING_FIRMWARE", null ],
      [ "Programming", "GETTING_STARTED.html#PROGRAMMING_FIRMWARE", [
        [ "Launching Miniterm", "GETTING_STARTED.html#autotoc_md66", null ],
        [ "Launching OpenOCD", "GETTING_STARTED.html#GETTING_STARTED_OPENOCD", null ],
        [ "Setting Application Configuration", "GETTING_STARTED.html#SET_APP_CONFIGURATION", [
          [ "Additional steps for the mm-ekh08-wb55 platform", "GETTING_STARTED.html#autotoc_md67", null ]
        ] ],
        [ "Launching GDB", "GETTING_STARTED.html#autotoc_md68", null ]
      ] ],
      [ "Troubleshooting", "GETTING_STARTED.html#autotoc_md69", [
        [ "Unable to find BCF file entry in config store", "GETTING_STARTED.html#BCF_CONFIG_STORE", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md70", null ],
          [ "Possible cause", "GETTING_STARTED.html#autotoc_md71", null ]
        ] ],
        [ "Unable to connect to the MCU with OpenOCD", "GETTING_STARTED.html#autotoc_md72", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md73", null ],
          [ "Possible cause 1", "GETTING_STARTED.html#autotoc_md74", null ],
          [ "Possible cause 2", "GETTING_STARTED.html#autotoc_md75", null ]
        ] ]
      ] ]
    ] ],
    [ "Host Power Save", "MMHOSTPOWERSAVE.html", [
      [ "Overview", "MMHOSTPOWERSAVE.html#autotoc_md76", null ],
      [ "Implementation", "MMHOSTPOWERSAVE.html#autotoc_md77", null ],
      [ "Deep sleep decision logic", "MMHOSTPOWERSAVE.html#autotoc_md78", null ],
      [ "Deep sleep exceptions", "MMHOSTPOWERSAVE.html#autotoc_md79", null ],
      [ "Debugging during deep sleep", "MMHOSTPOWERSAVE.html#MMHOSTPOWERSAVE_DEBUG_DEEP_SLEEP", null ]
    ] ],
    [ "Morse Micro CLI API", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html", [
      [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md80", null ],
      [ "Wireless LAN management (wlan) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md81", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md82", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md83", [
          [ "wlan-connect", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md84", null ],
          [ "wlan-disconnect", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md85", null ],
          [ "wlan-scan", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md86", null ],
          [ "wlan-get_rssi", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md87", null ],
          [ "wlan-get_mac_addr", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md88", null ],
          [ "wlan-wnm_sleep", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md89", null ],
          [ "wlan-beacon_monitor_enable", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md90", null ],
          [ "wlan-beacon_monitor_disable", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md91", null ],
          [ "wlan-standby_enter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md92", null ],
          [ "wlan-standby_exit", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md93", null ],
          [ "wlan-standby_set_status_payload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md94", null ],
          [ "wlan-standby_set_wake_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md95", null ],
          [ "wlan-standby_set_config", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md96", null ]
        ] ]
      ] ],
      [ "IP Stack Management (ip) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md97", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md98", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md99", [
          [ "ip-status", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md100", null ],
          [ "ip-reload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md101", null ],
          [ "ip-enable_tcp_keepalive_offload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md102", null ],
          [ "ip-disable_tcp_keepalive_offload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md103", null ],
          [ "ip-set_whitelist_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md104", null ],
          [ "ip-clear_whitelist_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md105", null ]
        ] ]
      ] ],
      [ "Ping application (ping) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md106", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md107", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md108", [
          [ "ping-run", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md109", null ]
        ] ]
      ] ],
      [ "Iperf application (iperf) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md110", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md111", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md112", [
          [ "iperf-run", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md113", null ]
        ] ]
      ] ],
      [ "System management (sys) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md114", [
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md115", [
          [ "sys-reset", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md116", null ],
          [ "sys-deep_sleep", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md117", null ],
          [ "sys-get_version", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md118", null ]
        ] ]
      ] ],
      [ "Enum definitions", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md119", [
        [ "Security type", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_security_type", null ],
        [ "Pmf mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_pmf_mode", null ],
        [ "Power save mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_power_save_mode", null ],
        [ "Station type", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_station_type", null ],
        [ "Status", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_status", null ],
        [ "Iperf mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_iperf_mode", null ],
        [ "Iperf state", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_iperf_state", null ],
        [ "Ip link state", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_ip_link_state", null ],
        [ "Deep sleep mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_deep_sleep_mode", null ],
        [ "Standby mode exit reason", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_standby_mode_exit_reason", null ]
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
"GETTING_STARTED.html",
"emmet_8h_source.html",
"group__MMAGIC__CONTROLLER__INTERNAL.html#gga3fdd29b88bd1bef6ea91cd677513167aa4907722961673c21bdce293864efe91d",
"group__MMCONFIG__API.html#gga3f0ca3ae6640a9e5436d2b7c89d36f17a001e24f95fa5b2a7647e02e3870b2329",
"group__MMOSAL__MEMMGMT.html#ga022c5f3377a730207f7b709770f3201b",
"group__MMWLAN__DATA.html#ga07e4f93f5563a05beee49e3a8731c194",
"mm__app__common_8h_source.html",
"structmmagic__core__wlan__connect__cmd__args.html",
"structmmwlan__sta__args.html#a203987678cafb13e43d58f70564d78a2",
"wnm__sleep_8c.html#afb92bea4a697b14d9a1116d58bda79c8"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';