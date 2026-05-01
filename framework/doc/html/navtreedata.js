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
    [ "Morse Micro CLI API", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html", [
      [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md72", null ],
      [ "Module wlan: Wireless LAN management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md73", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md74", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md75", [
          [ "wlan-connect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md76", null ],
          [ "wlan-disconnect", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md77", null ],
          [ "wlan-scan", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md78", null ],
          [ "wlan-get_rssi", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md79", null ],
          [ "wlan-get_mac_addr", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md80", null ],
          [ "wlan-wnm_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md81", null ],
          [ "wlan-beacon_monitor_enable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md82", null ],
          [ "wlan-beacon_monitor_disable", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md83", null ],
          [ "wlan-get_sta_status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md84", null ],
          [ "wlan-dpp_push_button_start", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md85", null ],
          [ "wlan-dpp_stop", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md86", null ]
        ] ]
      ] ],
      [ "Module ip: IP Stack Management", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md87", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md88", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md89", [
          [ "ip-status", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md90", null ],
          [ "ip-reload", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md91", null ]
        ] ]
      ] ],
      [ "Module ping: Ping application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md92", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md93", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md94", [
          [ "ping-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md95", null ]
        ] ]
      ] ],
      [ "Module iperf: Iperf application.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md96", [
        [ "Configuration variables", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md97", null ],
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md98", [
          [ "iperf-run", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md99", null ]
        ] ]
      ] ],
      [ "Module sys: System management.", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md100", [
        [ "Commands", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md101", [
          [ "sys-reset", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md102", null ],
          [ "sys-deep_sleep", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md103", null ],
          [ "sys-get_version", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md104", null ],
          [ "sys-get_stats", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md105", null ]
        ] ]
      ] ],
      [ "Enum definitions", "md__home_jenkins_agent_workspace_build_mhs_binaries_mhs_os_mmagic_autogen_mmagic_cli.html#autotoc_md106", [
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
"DEBUGGING.html",
"dns__client_8c_source.html",
"group__MMAGIC__CONTROLLER__INTERNAL.html#gga64b6394bbbc22303cd3297d10f1da48ea74022bdc4ffe7c51110f3f393ef20372",
"group__MMBUF.html#gae66ee5aebe73bf16257d46ece7c2bfd1",
"group__MMIPERF.html#ga7b67e7f7ee9517307f9037835fc7f2ee",
"group__MMUTILS__WLAN.html#ga0e68dafaaae1487f57f31513e8a0b423",
"group__MMWLAN__STATS.html#gab6c2dc43ea23189256d14019e0b04c1c",
"ping_8c.html#af6267920b504273777e2451780e565afad80fa7cbc3a54f74032429011ccaabcf",
"structmmagic__core__wlan__beacon__monitor__enable__cmd__args.html",
"structmmwlan__s1g__channel.html#a439d35c1296a95736a93b0b1fb1cd4f3",
"udp__broadcast_8c.html#aa90ed4014a2f933e5220ad4860b0bdeba736237ce367b7814c763eacd188cf9e5"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';