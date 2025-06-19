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
      [ "Prerequisites", "GETTING_STARTED.html#autotoc_md57", null ],
      [ "Unpacking the software package", "GETTING_STARTED.html#UNPACKING_THE_SW", null ],
      [ "Package contents", "GETTING_STARTED.html#autotoc_md58", null ],
      [ "Development PC setup", "GETTING_STARTED.html#GETTING_STARTED_DEV_PC_SETUP", [
        [ "Python environment", "GETTING_STARTED.html#autotoc_md59", null ]
      ] ],
      [ "AP configuration", "GETTING_STARTED.html#autotoc_md60", null ],
      [ "Application overview", "GETTING_STARTED.html#autotoc_md61", [
        [ "Ping Example Application Structure", "GETTING_STARTED.html#autotoc_md62", null ],
        [ "Application Configuration", "GETTING_STARTED.html#APP_CONFIGURATION", [
          [ "MMConfig Application configuration", "GETTING_STARTED.html#autotoc_md63", null ],
          [ "Legacy Application Configuration", "GETTING_STARTED.html#autotoc_md64", null ]
        ] ]
      ] ],
      [ "Building Firmware", "GETTING_STARTED.html#BUILDING_FIRMWARE", null ],
      [ "Programming", "GETTING_STARTED.html#PROGRAMMING_FIRMWARE", [
        [ "Launching Miniterm", "GETTING_STARTED.html#autotoc_md65", null ],
        [ "Launching OpenOCD", "GETTING_STARTED.html#GETTING_STARTED_OPENOCD", null ],
        [ "Setting Application Configuration", "GETTING_STARTED.html#SET_APP_CONFIGURATION", [
          [ "Additional steps for the mm-ekh08-wb55 platform", "GETTING_STARTED.html#autotoc_md66", null ]
        ] ],
        [ "Launching GDB", "GETTING_STARTED.html#autotoc_md67", null ]
      ] ],
      [ "Troubleshooting", "GETTING_STARTED.html#autotoc_md68", [
        [ "Unable to find BCF file entry in config store", "GETTING_STARTED.html#BCF_CONFIG_STORE", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md69", null ],
          [ "Possible cause", "GETTING_STARTED.html#autotoc_md70", null ]
        ] ],
        [ "Unable to connect to the MCU with OpenOCD", "GETTING_STARTED.html#autotoc_md71", [
          [ "Symptom", "GETTING_STARTED.html#autotoc_md72", null ],
          [ "Possible cause 1", "GETTING_STARTED.html#autotoc_md73", null ],
          [ "Possible cause 2", "GETTING_STARTED.html#autotoc_md74", null ]
        ] ]
      ] ]
    ] ],
    [ "Host Power Save", "MMHOSTPOWERSAVE.html", [
      [ "Overview", "MMHOSTPOWERSAVE.html#autotoc_md75", null ],
      [ "Implementation", "MMHOSTPOWERSAVE.html#autotoc_md76", null ],
      [ "Deep sleep decision logic", "MMHOSTPOWERSAVE.html#autotoc_md77", null ],
      [ "Deep sleep exceptions", "MMHOSTPOWERSAVE.html#autotoc_md78", null ],
      [ "Debugging during deep sleep", "MMHOSTPOWERSAVE.html#MMHOSTPOWERSAVE_DEBUG_DEEP_SLEEP", null ]
    ] ],
    [ "Morse Micro CLI API", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html", [
      [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md79", null ],
      [ "Wireless LAN management (wlan) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md80", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md81", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md82", [
          [ "wlan-connect", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md83", null ],
          [ "wlan-disconnect", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md84", null ],
          [ "wlan-scan", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md85", null ],
          [ "wlan-get_rssi", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md86", null ],
          [ "wlan-get_mac_addr", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md87", null ],
          [ "wlan-wnm_sleep", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md88", null ],
          [ "wlan-beacon_monitor_enable", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md89", null ],
          [ "wlan-beacon_monitor_disable", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md90", null ],
          [ "wlan-standby_enter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md91", null ],
          [ "wlan-standby_exit", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md92", null ],
          [ "wlan-standby_set_status_payload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md93", null ],
          [ "wlan-standby_set_wake_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md94", null ],
          [ "wlan-standby_set_config", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md95", null ]
        ] ]
      ] ],
      [ "IP Stack Management (ip) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md96", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md97", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md98", [
          [ "ip-status", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md99", null ],
          [ "ip-reload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md100", null ],
          [ "ip-enable_tcp_keepalive_offload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md101", null ],
          [ "ip-disable_tcp_keepalive_offload", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md102", null ],
          [ "ip-set_whitelist_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md103", null ],
          [ "ip-clear_whitelist_filter", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md104", null ]
        ] ]
      ] ],
      [ "Ping application (ping) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md105", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md106", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md107", [
          [ "ping-run", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md108", null ]
        ] ]
      ] ],
      [ "Iperf application (iperf) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md109", [
        [ "Configuration variables", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md110", null ],
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md111", [
          [ "iperf-run", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md112", null ]
        ] ]
      ] ],
      [ "System management (sys) module", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md113", [
        [ "Commands", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md114", [
          [ "sys-reset", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md115", null ],
          [ "sys-deep_sleep", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md116", null ],
          [ "sys-get_version", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md117", null ]
        ] ]
      ] ],
      [ "Enum definitions", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md118", [
        [ "Security type", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_security_type", null ],
        [ "Pmf mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_pmf_mode", null ],
        [ "Power save mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_power_save_mode", null ],
        [ "Mcs10 mode", "md__root_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#CLI_enum_mcs10_mode", null ],
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
"files.html",
"group__MMAGIC__CONTROLLER__INTERNAL.html#gga62fb515a7270a1a2eec77f80cd4fb9b7a5ea49822d188f4a62d2cb61066726e00",
"group__MMHAL.html#ga38da43831c553f238544c541c94b0e44",
"group__MMOSAL__MUTEX.html#gadf9da47930d58b6f893abb3c32b8f899",
"group__MMWLAN__DATA.html#ga6501ac548a4ed7860471238854f269ed",
"mm__app__common_8h_source.html",
"structmmagic__core__wlan__scan__cmd__args.html#a8cf308c111c34508129392ac2a22f0e7",
"structmmwlan__sta__args.html#a273e51fd31b3331e73b33f2c3d3de775",
"wnm__sleep_8c.html#af6267920b504273777e2451780e565afa4a6c9b049bc6a49d7ce94ecb1e5e7d9a"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';