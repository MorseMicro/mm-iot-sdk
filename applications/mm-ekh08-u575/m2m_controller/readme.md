mm-ekh08-u575 SPI Readme {#MM_EKH08_U575_README}
====

__Copyright 2022-2024 Morse Micro__

# Summary

This directory contains all of the hardware dependent code needed to use a STM32 Nucleo-144
development board with STM32U575ZITxQ MCU.

The `bsp` contains all of the setup and configuration code for the board. This will need
to be modified to select and provide initialisation endpoints for the peripherals. A good starting
point for this is to use the [STM32CubeMX v6.11.0](https://www.st.com/stm32cubemx) which can be
used to generate starting code for the board/chip that you wish to implement.

> Note that the files in the `bsp` directory have been modifications from the versions originally
> generated by STM32CubeMX. In particular is important to note that the
> `bsp/STM32U575ZITXQ_FLASH.ld` and `bsp/startup_stm32u575zitxq.s` have been modified.

The files in the `mm_shims` directory (e.g., `mmhal.c`, `mmport.h`, `wlan_hal.c`) are where the
board-specific API functions used by morselib and other software components are implemented.
