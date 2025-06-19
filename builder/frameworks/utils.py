#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import errno
import glob
import logging
import os
import re
import socket
import subprocess
import time

MAX_CONNECTION_ATTEMPTS = 5


class MakefileParseError(Exception):
    def __init__(self, filename, linenum, msg):
        super().__init__(f"{filename}:{linenum}: {msg}")
        self.filename = filename
        self.linenum = linenum
        self.raw_msg = msg


class MakeFragmentParser:
    """
    This is a very rudimentry parser for makefile fragments that extracts the contents of
    variables are returns them as a dictionary. It makes a number of simplifications in its
    parsing, since it is only intended to handle a specific set of hand crafted fragments.

    Things that are explicitly not supported:
    - # for any purpose other than comment
    - Spaces in variables (except for filter-out macro's arg)
    - Any macro other than addprefix, include, patsubst (% at end of pattern only), wildcard
    - $$

    Simplifications:
    - Variable expansion is probably not done correctly for the likes of include and patsubst
    """

    # Warning: these are very simplistic and requires no commas in the expressions being compared
    IFEQ_RE = re.compile(r"ifeq\s*\((?P<exprl>.*),\s*(?P<exprr>.*)\)")
    IFNEQ_RE = re.compile(r"ifneq\s*\((?P<exprl>.*),\s*(?P<exprr>.*)\)")
    INCLUDE_RE = re.compile(r"include\s+(?P<filename>[^\s]+)\s*")
    ERROR_RE = re.compile(r"\$\(error\s+(?P<errormsg>.*)\s*\)")

    def __init__(self, env_vars={}):
        self.mk_vars = env_vars.copy()

        # Remove any 'None' environment variables
        # Note that env_vars and self.mk_vars start out the same, so we can iterate env_vars to
        # avoid a `dictionary changed size during iteration` error.
        for k, v in env_vars.items():
            if v is None:
                self.mk_vars.pop(k)

    def _parse_mk_file(self, mk_filename, mk_file):
        line_num = 0
        line_continuation = ""
        conditional_stack = []

        for line in mk_file:
            line_num += 1
            # Remove anything after the first #
            # This is pretty naive and won't deal with # in string, but should be OK for our simple
            # use case.
            line = line.split("#", maxsplit=1)[0].strip()

            if line.endswith("\\"):
                line_continuation += line
                continue

            line = line_continuation + line
            line_continuation = ""

            line = line.strip()

            if not line:
                continue

            ifeq_m = self.IFEQ_RE.match(line)
            if ifeq_m:
                exprl = self._eval_mk_vars(ifeq_m.group("exprl"), mk_filename, line_num)
                exprr = self._eval_mk_vars(ifeq_m.group("exprr"), mk_filename, line_num)

                conditional_stack.append(exprl == exprr)
                continue

            ifneq_m = self.IFNEQ_RE.match(line)
            if ifneq_m:
                exprl = self._eval_mk_vars(ifneq_m.group("exprl"), mk_filename, line_num)
                exprr = self._eval_mk_vars(ifneq_m.group("exprr"), mk_filename, line_num)

                conditional_stack.append(exprl != exprr)
                continue

            if line == "else":
                try:
                    current_conditional = conditional_stack.pop()
                except IndexError:
                    raise MakefileParseError(mk_filename, line_num, "else without ifeq/ifneq")
                conditional_stack.append(not current_conditional)
                continue

            if line == "endif":
                try:
                    conditional_stack.pop()
                except IndexError:
                    raise MakefileParseError(mk_filename, line_num, "endif without ifeq/ifneq")
                continue

            # If we have encountered a negative conditional then do not do any further processing
            if False in conditional_stack:
                continue

            include_m = self.INCLUDE_RE.match(line)
            if include_m:
                filename = self._eval_mk_vars(include_m.group("filename"), mk_filename, line_num)[0]
                with open(filename, "r") as f:
                    self._parse_mk_file(filename, f)
                continue

            error_m = self.ERROR_RE.match(line)
            if error_m:
                errormsg = error_m.group("errormsg")
                raise MakefileParseError(mk_filename, line_num, errormsg)

            tokens = [x.strip() for x in line.split("+=", maxsplit=1)]
            if len(tokens) == 2:
                key = self._eval_mk_vars(tokens[0], mk_filename, line_num)[0]
                var = self.mk_vars.setdefault(key, [])
                var += self._eval_mk_vars(tokens[1], mk_filename, line_num)
                continue

            tokens = [x.strip() for x in line.split("?=", maxsplit=1)]
            if len(tokens) == 2:
                key = self._eval_mk_vars(tokens[0], mk_filename, line_num)[0]
                if key not in self.mk_vars:
                    self.mk_vars[key] = self._eval_mk_vars(tokens[1], mk_filename, line_num)
                continue

            tokens = [x.strip() for x in line.split(":=", maxsplit=1)]
            if len(tokens) == 2:
                key = self._eval_mk_vars(tokens[0], mk_filename, line_num)[0]
                self.mk_vars[key] = self._eval_mk_vars(tokens[1], mk_filename, line_num)
                continue

            tokens = [x.strip() for x in line.split("=", maxsplit=1)]
            if len(tokens) == 2:
                key = self._eval_mk_vars(tokens[0], mk_filename, line_num)[0]
                self.mk_vars[key] = self._eval_mk_vars(tokens[1], mk_filename, line_num)
                continue

            logging.warn("Unable to parse line %d of %s: %s -- skipping",
                         line_num, mk_filename, line)

    # Given a space separate list of variables, recursively expand any variables ($(xxx)) and
    # return as an array of values.
    def _eval_mk_vars(self, mk_vars_str, mk_filename, line_num):
        var_start = False
        eval_stack = [""]
        for ch in mk_vars_str:
            if var_start:
                var_start = False
                if ch == "(":
                    eval_stack.append("")
                    continue
                else:
                    raise MakefileParseError(mk_filename, line_num,
                                             f"Expect ( after $ (got {ch})")
            if ch == "$":
                var_start = True
                continue
            if ch == ")":
                if len(eval_stack) < 2:
                    raise MakefileParseError(mk_filename, line_num, "Too many )")
                var_to_eval = eval_stack.pop()

                var_name = var_to_eval

                # Some limited special handling for macros
                var_name_tokens = var_name.split(" ")
                if var_name_tokens[0] == "addprefix":
                    prefix, first_var = var_name_tokens[1].split(",")
                    eval_stack[-1] += prefix + first_var
                    for var in var_name_tokens[2:]:
                        eval_stack[-1] += " " + prefix + var
                elif var_name_tokens[0] == "wildcard":
                    for path in var_name_tokens[1:]:
                        result = " ".join([os.path.basename(x) for x in glob.glob(path)])
                        eval_stack[-1] += result
                elif var_name_tokens[0] == "filter-out":
                    patterns, text = " ".join(
                        var_name_tokens[1:]).split(",", maxsplit=1)
                    text_tokens = text.split()
                    patterns = patterns.split(" ")
                    for pattern in patterns:
                        text_tokens.remove(pattern)
                    result = " ".join(text_tokens)
                    eval_stack[-1] += result
                elif var_name_tokens[0] == "patsubst":
                    pattern, replacement, text = " ".join(
                        var_name_tokens[1:]).split(",", maxsplit=2)
                    # This is a hacky implementation to meet our needs
                    # Only supports % at the end of pattern
                    if not pattern.endswith("%"):
                        raise MakefileParseError(
                            mk_filename, line_num,
                            "Simplified patsubst implementation only handles "
                            "% at end of pattern")
                    pattern = pattern[:-1]
                    if replacement != "%":
                        raise MakefileParseError(
                            mk_filename, line_num,
                            'Simplified patsubst implementation only handles '
                            'replacement string "%"')
                    text_tokens = text.split()
                    result_tokens = []
                    for token in text_tokens:
                        if token.startswith(pattern):
                            token = token[len(pattern):]
                        result_tokens.append(token)
                    result = " ".join(result_tokens)
                    eval_stack[-1] += result
                elif len(var_name_tokens) == 1:
                    try:
                        val = self.mk_vars[var_name]
                        if not isinstance(self.mk_vars[var_name], str):
                            val = " ".join(val)
                    except KeyError:
                        logging.info("Variable %s not defined, defaulting to empty", var_name)
                        val = ""
                    eval_stack[-1] += val
                else:
                    raise MakefileParseError(mk_filename, line_num,
                                             f"Unsupported macro {var_name_tokens[0]}")
                continue
            eval_stack[-1] += ch
        if len(eval_stack) != 1:
            raise MakefileParseError(mk_filename, line_num,
                                     f"Stack size {len(eval_stack)} at end -- {eval_stack})")
        return eval_stack[0].split()

    def dump(self):
        for k, v in self.mk_vars.items():
            print(f"{k+':':30s} {v}")


def import_mm_iot_components(env, base_dir, components, env_vars):
    included_components = []

    mk_parser = MakeFragmentParser(env_vars)

    for component in components:
        component = mk_parser._eval_mk_vars(component, "", 0)[0]

        # Don't include a component more than onc
        if component in included_components:
            continue

        filename = os.path.join(base_dir, "mk", f"{component}.mk")
        with open(filename, "r") as f:
            mk_parser._parse_mk_file(filename, f)

        included_components.append(component)

    per_file_cflags = {}
    for k, v in mk_parser.mk_vars.items():
        if k.startswith("CFLAGS-"):
            filename = k[len("CFLAGS-"):]
            per_file_cflags.setdefault(filename, []).extend(v)

    if "BUILD_DEFINES" in mk_parser.mk_vars:
        env.Append(CCFLAGS=["-D" + define for define in mk_parser.mk_vars["BUILD_DEFINES"]])
    if "BSP_LD_FILES" in mk_parser.mk_vars:
        if len(mk_parser.mk_vars["BSP_LD_FILES"]) > 1:
            raise MakefileParseError("", 0, "Only one LD file may be specified")
        env.Replace(LDSCRIPT_PATH=os.path.join(base_dir, mk_parser.mk_vars["BSP_LD_FILES"][0]))
    if "LINKFLAGS" in mk_parser.mk_vars:
        env.Append(LINKFLAGS=mk_parser.mk_vars["LINKFLAGS"])
    if "MMIOT_INCLUDES" in mk_parser.mk_vars:
        env.Append(CPPPATH=[os.path.join(base_dir, inc_path)
                            for inc_path in mk_parser.mk_vars["MMIOT_INCLUDES"]])
    if "MMIOT_SRCS_C" in mk_parser.mk_vars:
        build_files = env.CollectBuildFiles(
            os.path.join("$BUILD_DIR", "mmiot"),
            base_dir,
            src_filter=["-<*>"] + [f"+<{c_file}>" for c_file in mk_parser.mk_vars["MMIOT_SRCS_C"]])
        objs = []
        for node in build_files:
            custom_cflags = []
            for path, cflags in per_file_cflags.items():
                if str(node).startswith(os.path.join(base_dir, path)):
                    custom_cflags += cflags
            objs += env.Object(node, CFLAGS=env["CFLAGS"] + custom_cflags)
        env.Append(PIOBUILDFILES=objs)
    if "MMIOT_SRCS_S" in mk_parser.mk_vars:
        env.BuildSources(
            os.path.join("$BUILD_DIR", "mmiot"),
            base_dir,
            src_filter=["-<*>"] + [f"+<{s_file}>" for s_file in mk_parser.mk_vars["MMIOT_SRCS_S"]]
        )


def import_morse_fw_bin(env, base_dir, component):
    """
    Convert Morse firmware binary into an object file that we can link
    """
    MORSE_FW_RE = re.compile(r"^FW_MBIN.*?([\w\-\.]+\.mbin)", re.MULTILINE)
    fw_makefile = os.path.join(base_dir, "mk", f"{component}.mk")
    fw_mbin = ""
    with open(fw_makefile, "r") as f:
        match = re.search(MORSE_FW_RE, f.read())
        fw_mbin = match.group(1) if match else ""

    morse_fw_bin = os.path.join(base_dir, "morsefirmware", fw_mbin)
    if not os.path.isfile(morse_fw_bin):
        raise FileNotFoundError(f"Morse FW mbin not found: {morse_fw_bin}")

    morse_fw_obj = env.Command(
        os.path.join("$BUILD_DIR", "mmfw.o"),
        morse_fw_bin,
        action="$OBJCOPY -I binary -O elf32-littlearm -B ARM $SOURCE $TARGET "
               "--redefine-sym _binary_${PATH_IDENTIFIER}_start=firmware_binary_start "
               "--redefine-sym _binary_${PATH_IDENTIFIER}_end=firmware_binary_end "
               "--rename-section .data=.rodata,CONTENTS,ALLOC,LOAD,READONLY,DATA",
        PATH_IDENTIFIER=re.sub(r"[^a-zA-Z0-9]", "_", morse_fw_bin)
    )
    env.Append(PIOBUILDFILES=[morse_fw_obj])


def openocd_start(env):
    platform = env.PioPlatform()
    board = env.BoardConfig()

    upload_protocol = env.subst("$UPLOAD_PROTOCOL")
    debug_tools = board.get("debug.tools", {})
    if upload_protocol in debug_tools:
        package_dir = debug_tools.get(upload_protocol).get("server").get("package")
        package_dir = platform.get_package_dir(package_dir) or ""
        openocd_args = debug_tools.get(upload_protocol).get("server").get("arguments", [])
        openocd_args = [
            f.replace("$PACKAGE_DIR", package_dir)
            for f in openocd_args
        ]
        openocd_args = [env.subst(a) for a in openocd_args]

        print(["openocd", *openocd_args])
        proc = subprocess.Popen(["openocd", *openocd_args])

        connect_attempts = 0
        while connect_attempts < MAX_CONNECTION_ATTEMPTS:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(10)
                sock.connect(("localhost", 6666))
            except OSError as err:
                if err.errno == errno.ECONNREFUSED:
                    connect_attempts += 1
                    sock.close()
                    logging.warning("OpenOCD connection refused, not up yet? "
                                    f"Trying again in {connect_attempts} seconds")
                    time.sleep(connect_attempts)
                else:
                    logging.error(err)
                    sock.close()
                    raise
            except Exception as err:
                logging.error(err)
                sock.close()
                proc.kill()
                raise err
            else:
                sock.close()
                break

        if connect_attempts >= MAX_CONNECTION_ATTEMPTS:
            logging.error("OpenOCD connection refused.")
            raise OSError(errno.ECONNREFUSED, "Connection refused")

        return proc
    return None


def program_configstore(env, *_args, **_kwargs):
    platform = env.PioPlatform()
    proc = openocd_start(env)

    config_store = os.path.join(platform.get_dir(),
                                "framework", "tools",
                                "platform",
                                "program-configstore.py")
    config_store_args = "-H localhost -p $PIOENV write-json config.hjson"
    env.Replace(
        CONFIGSTORE=config_store,
        CONFIGSTOREFLAGS=config_store_args,
        CONFIGSTORECMD='"$PYTHONEXE" "$CONFIGSTORE" $CONFIGSTOREFLAGS'
    )

    env.Execute(env.VerboseAction("$CONFIGSTORECMD", "Programming config store $CONFIGSTORECMD"))

    proc.kill()


def erase_configstore(env, *_args, **_kwargs):
    platform = env.PioPlatform()
    proc = openocd_start(env)

    config_store = os.path.join(platform.get_dir(),
                                "framework", "tools",
                                "platform",
                                "program-configstore.py")
    config_store_args = "-H localhost -p $PIOENV erase --all"
    env.Replace(
        CONFIGSTORE=config_store,
        CONFIGSTOREFLAGS=config_store_args,
        CONFIGSTORECMD='"$PYTHONEXE" "$CONFIGSTORE" $CONFIGSTOREFLAGS'
    )

    env.Execute(env.VerboseAction("$CONFIGSTORECMD", "Erasing config store $CONFIGSTORECMD"))

    proc.kill()
