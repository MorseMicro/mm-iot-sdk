#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

import os

# Load Ststm32Platform class from the included python file so that we can subclass it.
from platformio.compat import load_python_module

stsm32platform = load_python_module("platformio.platform.Ststm32Platform",
                                    __file__.replace("platform.py", "platform_ststm32.py"))
Ststm32Platform = getattr(stsm32platform, "Ststm32Platform")


class Mm_iottoolkitPlatform(Ststm32Platform):  # noqa: N801

    def configure_default_packages(self, variables, targets):
        board = variables.get("board")
        board_config = self.board_config(board)
        variables.get("board_build.mcu", board_config.get("build.mcu", ""))

        frameworks = variables.get("pioframework", [])
        if "mm_iot" in frameworks:
            # Here is where we can add any framework dependencies.
            pass

        return Ststm32Platform.configure_default_packages(self, variables, targets)

    def get_boards(self, id_=None):
        result = Ststm32Platform.get_boards(self, id_)
        if not result:
            return result
        if id_:
            return self._add_default_openocd_cfg(result)
        else:
            for key, value in result.items():
                result[key] = self._add_default_openocd_cfg(result[key])
        return result

    def _add_default_openocd_cfg(self, board):
        debug = board.manifest.get("debug", {})
        upload_protocols = board.manifest.get("upload", {}).get(
            "protocols", [])

        cfg_file_path = os.path.join(self.get_dir(), "framework",
                                     "src", "platforms",
                                     board.id, "openocd.cfg")

        # just exit if tools hasn't been pre-configured by ststm32
        if "tools" not in debug:
            return board

        # openocd targets
        for link in ("stlink", "cmsis-dap"):
            if link not in upload_protocols:
                continue
            debug_config = debug["tools"][link] or {}
            debug_config["load_cmds"] = ["monitor reset halt", "load"]
            debug_config["init_break"] = "tbreak app_init"
            args = debug_config["server"]["arguments"]

            if cfg_file_path in args:
                continue

            args.extend(["-f", cfg_file_path])

        board.manifest["debug"] = debug
        return board
