#!/usr/bin/env python3
#
# Copyright 2023-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

'''
This script autogenerates the core API for the MM CLI.
'''

import argparse
import logging
import os
import re
import jinja2
from mmagiclib import api

# ---------- Custom Jinja2 filter methods ---------- #


def regex_replace(s, find, replace):
    return re.sub(find, replace, s)


class CodeGenerator:
    def __init__(self, yaml_file, templates_dir, output_dir):
        self.output_dir = output_dir
        self.config = api.Configuration.load(yaml_file)
        self.template_env = jinja2.Environment(loader=jinja2.FileSystemLoader(templates_dir),
                                               keep_trailing_newline=True)
        self.template_env.filters['regex_replace'] = regex_replace
        self.output_filenames = []

    def write_c(self, output_filename, c_template, data=None):
        if os.path.splitext(output_filename)[-1] in [".c", ".h"]:
            self.output_filenames.append(output_filename)
        with open(output_filename, 'w') as outf:
            outf.write(c_template.render(config=self.config, data=data))

    def _generate_types(self, output_dir):
        template = self.template_env.get_template('core/types.h.template')
        output_file = os.path.join(output_dir, 'mmagic_core_types.h')
        self.write_c(output_file, template)
        template = self.template_env.get_template('core/types.c.template')
        output_file = os.path.join(output_dir, 'mmagic_core_types.c')
        self.write_c(output_file, template)

    def _generate_data_header(self, output_dir):
        template = self.template_env.get_template('core/data.h.template')
        output_file = os.path.join(output_dir, 'mmagic_core_data.h')
        self.write_c(output_file, template)

    def _generate_data_src(self, output_dir):
        template = self.template_env.get_template('core/data.c.template')
        output_file = os.path.join(output_dir, 'mmagic_core_data.c')
        self.write_c(output_file, template)

    def _generate_module_c(self, output_dir):
        template = self.template_env.get_template('core/module.c.template')
        for module in self.config.modules:
            output_file = os.path.join(output_dir, f'mmagic_core_autogen_{module.name}.c')
            self.write_c(output_file, template, module)

    def _generate_module_header(self, output_dir):
        template = self.template_env.get_template('core/module.h.template')
        for module in self.config.modules:
            output_file = os.path.join(output_dir, f'mmagic_core_{module.name}.h')
            self.write_c(output_file, template, module)

    def generate_core_files(self):
        agent_core_output_dir = os.path.join(self.output_dir, 'agent/core/autogen')
        os.makedirs(agent_core_output_dir, exist_ok=True)
        controller_core_output_dir = os.path.join(self.output_dir, 'controller/core/autogen')
        os.makedirs(controller_core_output_dir, exist_ok=True)

        # Generate headers, def files, and source files for agent
        self._generate_types(agent_core_output_dir)
        self._generate_data_header(agent_core_output_dir)
        self._generate_data_src(agent_core_output_dir)
        self._generate_module_c(agent_core_output_dir)
        self._generate_module_header(agent_core_output_dir)

    def _generate_cli_module_c(self, output_dir):
        template = self.template_env.get_template('cli/cli_module.c.template')
        for module in self.config.modules:
            if not module.cli_support:
                continue
            output_file = os.path.join(output_dir, f'mmagic_cli_autogen_{module.name}.c')
            self.write_c(output_file, template, module)

    def _generate_cli_module_header(self, output_dir):
        template = self.template_env.get_template('cli/cli_module.h.template')
        for module in self.config.modules:
            if not module.cli_support:
                continue
            output_file = os.path.join(output_dir, f'mmagic_cli_{module.name}.h')
            self.write_c(output_file, template, module)

    def _generate_cli_internal_header(self, output_dir):
        template = self.template_env.get_template('cli/cli_internal.h.template')
        output_file = os.path.join(output_dir, 'mmagic_cli_internal.h')
        self.write_c(output_file, template)

    def _generate_cli_c(self, output_dir):
        template = self.template_env.get_template('cli/mmagic_cli.c.template')
        output_file = os.path.join(output_dir, 'mmagic_cli_autogen.c')
        self.write_c(output_file, template)

    def generate_cli_files(self):
        cli_output_dir = os.path.join(self.output_dir, 'agent/cli/autogen')
        os.makedirs(cli_output_dir, exist_ok=True)

        self._generate_cli_module_c(cli_output_dir)
        self._generate_cli_module_header(cli_output_dir)
        self._generate_cli_internal_header(cli_output_dir)
        self._generate_cli_c(cli_output_dir)

    def _generate_controller_header(self, output_dir):
        template = self.template_env.get_template('controller/controller.h.template')
        output_file = os.path.join(output_dir, 'mmagic_controller.h')
        self.write_c(output_file, template)

    def _generate_controller_source(self, output_dir):
        template = self.template_env.get_template('controller/controller.c.template')
        output_file = os.path.join(output_dir, 'mmagic_controller.c')
        self.write_c(output_file, template)

    def generate_controller_files(self):
        controller_output_dir = os.path.join(self.output_dir, 'controller')
        os.makedirs(controller_output_dir, exist_ok=True)
        self._generate_controller_header(controller_output_dir)
        self._generate_controller_source(controller_output_dir)

    def _generate_m2m_module_source(self, output_dir):
        template = self.template_env.get_template('m2m/m2m_module.c.template')
        for module in self.config.modules:
            output_file = os.path.join(output_dir, f'mmagic_m2m_autogen_{module.name}.c')
            self.write_c(output_file, template, module)

    def _generate_m2m_module_header(self, output_dir):
        template = self.template_env.get_template('m2m/m2m_module.h.template')
        for module in self.config.modules:
            output_file = os.path.join(output_dir, f'mmagic_m2m_{module.name}.h')
            self.write_c(output_file, template, module)

    def _generate_m2m_internal_files(self, output_dir):
        template = self.template_env.get_template('m2m/m2m_internal.h.template')
        output_file = os.path.join(output_dir, 'mmagic_m2m_internal.h')
        self.write_c(output_file, template)

    def _generate_m2m_c(self, output_dir):
        template = self.template_env.get_template('m2m/mmagic_m2m.c.template')
        output_file = os.path.join(output_dir, 'mmagic_m2m_autogen.c')
        self.write_c(output_file, template)

    def generate_m2m_files(self):
        m2m_output_dir = os.path.join(self.output_dir, 'agent/m2m_api/autogen')
        os.makedirs(m2m_output_dir, exist_ok=True)

        self._generate_m2m_module_source(m2m_output_dir)
        self._generate_m2m_module_header(m2m_output_dir)
        self._generate_m2m_internal_files(m2m_output_dir)
        self._generate_m2m_c(m2m_output_dir)

    def generate_doc_files(self):
        make_output_dir = os.path.join(self.output_dir, 'autogen')
        os.makedirs(make_output_dir, exist_ok=True)
        template = self.template_env.get_template('doc/cli.md.template')
        output_file = os.path.join(make_output_dir, 'mmagic_cli.md')
        self.write_c(output_file, template)

    def generate_make_files(self):
        make_output_dir = os.path.join(self.output_dir, 'autogen')
        for device in ["agent", "controller"]:
            template = self.template_env.get_template(f'make/mmagic_{device}.mk.template')
            output_file = os.path.join(make_output_dir, f'mmagic_{device}.mk')
            self.write_c(output_file, template)

# ---------- Main APP ---------- #
def _app_setup_args(parser):
    parser.add_argument('-c', '--config-file', help='Path to config file')
    parser.add_argument('-o', '--output-dir', help='Path to output the generated files')
    parser.add_argument('-L', '--list-outputs',
                        help='Path to file to write list of generated C and H files to')


def _app_main(args):
    template_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'templates')

    with open(args.config_file, 'r') as f:
        codegen = CodeGenerator(f, template_dir, args.output_dir)

    codegen.generate_doc_files()
    codegen.generate_make_files()
    codegen.generate_core_files()
    codegen.generate_cli_files()
    codegen.generate_controller_files()
    codegen.generate_m2m_files()

    if args.list_outputs:
        with open(args.list_outputs, "w") as f:
            for filename in codegen.output_filenames:
                f.write(filename)
                f.write("\n")

#
# ---------------------------------------------------------------------------------------------
#

def _main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter,
                                     description=__doc__)
    parser.add_argument('-v', '--verbose', action='count', default=0,
                        help='Increase verbosity of log messages (repeat for increased verbosity)')
    parser.add_argument('-l', '--log-file',
                        help='Log to the given file as well as to the console')

    _app_setup_args(parser)

    args = parser.parse_args()

    # Configure logging
    log_handlers = [logging.StreamHandler()]
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))

    LOG_FORMAT = '%(asctime)s %(levelname)s: %(message)s'
    if args.verbose >= 2:
        logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT, handlers=log_handlers)
    elif args.verbose == 1:
        logging.basicConfig(level=logging.INFO, format=LOG_FORMAT, handlers=log_handlers)
    else:
        logging.basicConfig(level=logging.WARNING, format=LOG_FORMAT, handlers=log_handlers)

    # If coloredlogs package is installed, then use it to get pretty coloured log messages.
    # To install:
    #    pip3 install coloredlogs
    try:
        import coloredlogs
        coloredlogs.install(fmt=LOG_FORMAT, level=logging.root.level)
    except ImportError:
        logging.debug('coloredlogs not installed')

    _app_main(args)


if __name__ == '__main__':
    _main()
