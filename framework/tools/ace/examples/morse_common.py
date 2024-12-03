#
# Copyright 2021-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#
"""
Common functionality that is used across the example scripts
"""
import logging


def configure_logging(verbosity=0):
    """
    Configures the logging module

    Args:
    ----
        verbosity : int, optional
            Sets the verbosity of the logging messages.
            0            = Warning
            1            = Info
            2 or greater = Debug

    """
    log_handlers = [logging.StreamHandler()]

    LOG_FORMAT = "%(asctime)s %(levelname)s: %(message)s"
    if verbosity >= 2:
        logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT, handlers=log_handlers)
    elif verbosity == 1:
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
        logging.debug("coloredlogs not installed")
