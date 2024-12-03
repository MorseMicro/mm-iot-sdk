mm-ekh08-wb55 with BLE (mm-ekh08-wb55-ble) Readme {#MM_EKH08_WB55_BLE_README}
====

__Copyright 2023-2024 Morse Micro__

# Summary

This directory contains all of the hardware dependent code needed to use a STM32 Nucleo-64
development board with STM32WB55RG MCU configured for BLE operation.

> It should be noted that the provided cube.ioc is provided as a reference only. If code is
> regenerated using [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) it will
> break the applications. This is because STM32CubeMX does not support generating BLE code with
> FreeRTOS. Generated using [STM32CubeMX v6.6.1](https://www.st.com/stm32cubemx).

This platform is based off of the basic mm-ekh08-wb55rg platform with the addition of the [heart
rate sensor demo BLE
app](https://github.com/STMicroelectronics/STM32CubeWB/tree/v1.14.1/Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_HeartRateFreeRTOS)
provided by ST. It is meant to serve as a reference for running BLE & MM-IOT-SDK stack concurrently.
We have taken care to minimize the changes to make view the diff to both the ST application and our
non-BLE mm-ekh08-wb55rg platform possible.

The source for all the ST provided code is taken from the [STM32CubeWB
v1.14.1](https://github.com/STMicroelectronics/STM32CubeWB) repository

## Recommended Reading {#WB55BLE_Recommended_Reading}

It is highly recommended to read the following application notes provided by ST before developing
on this platform.

### General Documents
These are useful as a reference for the general operation of the MCU.
- [RM-434 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0434-multiprotocol-wireless-32bit-mcu-armbased-cortexm4-with-fpu-bluetooth-lowenergy-and-802154-radio-solution-stmicroelectronics.pdf)
- [Datasheet](https://www.st.com/resource/en/datasheet/stm32wb55cc.pdf)

## BLE Specific Documents
These documents go over the BLE portions of the MCU in greater detail.
- [Building wireless applications with STM32WB Series microcontrollers - AN5289](https://www.st.com/resource/en/application_note/an5289-building-wireless-applications-with-stm32wb-series-mcus-stmicroelectronics.pdf)
- [Guidelines for Bluetooth® Low Energy stack programming on STM32WB/STM32WBA MCUs - PM0271](https://www.st.com/resource/en/programming_manual/pm0271-guidelines-for-bluetooth-low-energy-stack-programming-on-stm32wb-stm32wba-mcus-stmicroelectronics.pdf)
- [ST firmware upgrade services for STM32WB Series - AN5185](https://www.st.com/resource/en/application_note/an5185-st-firmware-upgrade-services-for-stm32wb-series-stmicroelectronics.pdf)
- [STM32WB Bluetooth® Low Energy wireless interface - AN5270](https://www.st.com/resource/en/application_note/an5270-stm32wb-bluetooth-low-energy-wireless-interface-stmicroelectronics.pdf)

## Board Support Package (BSP) directory overview

### Core
The `Core` directory contains all of the setup and configuration code for the board. This will need
to be modified to select and provide initialization endpoints for the peripherals. The following
files are modified slightly for the BLE application but serve a similar purpose to the non-BLE BSP.

- Core/Inc/stm32wbxx_hal_conf.h    HAL configuration file
- Core/Inc/stm32wbxx_it.h          Interrupt handlers header file
- Core/Inc/main.h                  Header for main.c module
- Core/Src/stm32wbxx_it.c          Interrupt handlers
- Core/Src/main.c                  Main program
- Core/Src/system_stm32wbxx.c      stm32wbxx system source file

#### BLE Specific files
These are all files included specifically for the BLE portion of the application.
Care should be taken when modifying defines as not all will work but have been left
in to make running a diff with the reference application easier.

- Core/Inc/app_common.h            Header for all modules with common definition
- Core/Inc/app_conf.h              Parameters configuration file of the application
- Core/Inc/app_entry.h             Parameters configuration file of the application
- Core/Inc/hw_conf.h               Configuration file of the HW
- Core/Inc/utilities_conf.h        Configuration file of the utilities
- Core/Src/app_entry.c             Initialization of the application
- Core/Src/stm32_lpm_if.c          Low Power Manager Interface
- Core/Src/hw_timerserver.c        Timer Server based on RTC
- Core/Src/hw_uart.c               UART Driver

### STM32_WPAN
This directory contains all the logic for running the BLE application.

- STM32_WPAN/App/app_ble.h         Header for app_ble.c module
- STM32_WPAN/App/ble_conf.h        BLE Services configuration
- STM32_WPAN/App/ble_dbg_conf.h    BLE Traces configuration of the BLE services
- STM32_WPAN/App/dis_app.h         Header for dis_app.c module
- STM32_WPAN/App/hrs_app.h         Header for hrs_app.c module
- STM32_WPAN/App/app_ble.c         BLE Profile implementation
- STM32_WPAN/App/dis_app.c         Device Information Service application
- STM32_WPAN/App/hrs_app.c         Heart Rate Service application
- STM32_WPAN/Target/hw_ipcc.c      IPCC Driver

### mm_shims

The files in the `mm_shims` directory (`mmhal.c`, `mmport.h`, `wlan_hal.c`) are where the board
specific API functions used by morselib are implemented.

## Getting the BLE Stack running
This is a very basic overview on how to get the BLE stack running. It is highly recommend to review
the documents in the [Recommended Reading](#WB55BLE_Recommended_Reading) section for more details.

As an overview the STM32WB55RG has two CPU cores.

- CPU1 (ARM Cortex M4) is where the application code that we write will run.
- CPU2 (ARM Cortex M0+) is responsible for the BLE stack provided by ST. This is referred to as the
  wireless coprocessor.

### Loading the code.
For BLE operation ST has provided a number of pre-compiled binaries. For this example platform the
`stm32wb5x_BLE_Stack_light_fw.bin` binary has been used to minimize the amount of flash memory that
it uses. This binary is loaded in to the bottom portion of the flash using the firmware upgrade
services (FUS) available on STM32WB series MCUs. Instructions on how to load this can be found in
the [releases
notes](https://github.com/STMicroelectronics/STM32CubeWB/blob/v1.14.1/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x/Release_Notes.html)
for the coprocessor binaries. We provide a local copy on the necessary files here
`bsps/stm32cubewb/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x/`

Before loading the co-processor binary please ensure that you have the latest FUS FW version
installed, currently `v1.2.0`. Install steps for this can also be found in the release notes.
If this is not done first it will wipe the co-processor binary.

> The provided linker script has been configured to leave enough space for the
> `stm32wb5x_BLE_Stack_light_fw.bin` binary. If you wish to use another binary this will need to be
> adjusted.

Once the wireless coprocessor binary has been loaded using the steps provided by ST the example
applications can be loaded on as per the usual steps. Note that the wireless coprocessor binary
only needs to be loaded once.

### Running the code
Once the code has been load and starts running the applications will behave as per the other
platform. The BLE processes will have their own threads in the background that will handle running
the Heart Rate Monitor application.

For the BLE we can follow the steps provided by ST, [Heart Rate
Readme](https://github.com/STMicroelectronics/STM32CubeWB/blob/v1.14.1/Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_HeartRateFreeRTOS/readme.txt).

On the android/ios device, enable the Bluetooth communications, and if not done before,

- Install the ST BLE Sensor application on the ios/android device
    - https://play.google.com/store/apps/details?id=com.st.bluems&hl=en&gl=US&pli=1
    - https://apps.apple.com/us/app/st-ble-sensor/id993670214

- Power on the Nucleo board with the application
- Then, click on the App icon, ST BLE Sensor
- Connect to a device
- Select the HRSTM in the device list

You should now see a simulated heart beat value updating every 1 second.

### Debug Messages
The debug infrastructure provided by ST for the BLE module is not compatible with the rest of the
MM-IoT-SDK without significant changes. As stated above this has been left in to enabled easy
comparisons to the ST application.

That being said we have provided a method of printing the BLE debug messages. If you comment out the
define provided in `Core/Inc/app_conf.h` you will see the debug messages printed out the UART.

    /* Comment out the below line to enable debug logging for BLE */
    // #define APP_DBG_MSG(...)        do{printf("[%s]", "BLE");printf(__VA_ARGS__);}while(0);

## Pinout MMECH08

MM6108 pin | Nucleo/Arduino header pin | STM32WB55 pin
-----------|---------------------------|--------------
RESET_N    | A0                        | PC0
WAKE       | A1                        | PC1
BUSY       | A2                        | PA1
SPI_SCK    | SCK                       | PA5
SPI_MOSI   | MOSI                      | PA7
SPI_MISO   | MISO                      | PA6
SPI_CS     | CS                        | PA4
SPI_IRQ    | A4                        | PC3
