/**
*****************************************************************************
**
**  File        : stm32wb55xx_flash_cm4.ld
**
**  Abstract    : System Workbench Minimal System calls file
**
** 		          For more information about which c-functions
**                need which of these lowlevel functions
**                please consult the Newlib libc-manual
**
**  Environment : System Workbench for MCU
**
**  Distribution: The file is distributed “as is,” without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2019 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = 0x20030000;    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Specify the memory areas
 * Note that the flash size and config store location will be determined by which
 * Co-Processor wireless binary you have installed. This will be indicated by the
 * value of the SFSA register. You can read this with the OpenOCD command `mdb 0x1fff8070 1`.
 *
 * To install or delete the wireless binary use the STM32CubeProgrammer
 * https://www.st.com/en/development-tools/stm32cubeprog.html
 */
MEMORY
{
  RAM1          (xrw)   : ORIGIN = 0x20000008, LENGTH = 0x2FFF8
  RAM_SHARED    (xrw)   : ORIGIN = 0x20030000, LENGTH = 10K
  BOOTLOADER    (rx)    : ORIGIN = 0x08000000, LENGTH = 16K
  CONFIG        (rx)    : ORIGIN = 0x08004000, LENGTH = 16K
  /* The length of the FLASH section is dictated by the start address of the Wireless Coproc Binary.
   * Currently it is configured for stm32wb5x_BLE_Stack_light_fw.bin v1.14.1 which has a start
   * address of 0x080D5000 for the STM32WB5xxG(1M) */
  FLASH         (rx)    : ORIGIN = 0x08008000, LENGTH = 820K
}

/* Define output sections */
SECTIONS
{
  .mmconfig :
  {
    mmconfig_start  = ORIGIN(CONFIG);
    mmconfig_end    = ORIGIN(CONFIG) + LENGTH(CONFIG);
  }

  .application :
  {
    application_start  = ORIGIN(FLASH);
    application_end    = ORIGIN(FLASH) + LENGTH(FLASH);
  }

  /* The program code and other data goes into BOOTLOADER */
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.bootloader))     /* .bootloader sections (code) */
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >BOOTLOADER

  /* Constant data goes into BOOTLOADER */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >BOOTLOADER

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >BOOTLOADER
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >BOOTLOADER

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >BOOTLOADER
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >BOOTLOADER
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >BOOTLOADER

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM1 AT> BOOTLOADER


  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM1

  /* Uninitialized data section that is not zeroed at boot */
  .noinit :
  {
    *(.noinit)
    *(.noinit.*)
    . = ALIGN(4);
  } >RAM1

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM1

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    *(.isr_vector)
  }

  .ARM.attributes 0       : { *(.ARM.attributes) }
   MAPPING_TABLE (NOLOAD) : { *(MAPPING_TABLE) } >RAM_SHARED
   MB_MEM1 (NOLOAD)       : { *(MB_MEM1) } >RAM_SHARED
   MB_MEM2 (NOLOAD)       : { _sMB_MEM2 = . ; *(MB_MEM2) ; _eMB_MEM2 = . ; } >RAM_SHARED
}
