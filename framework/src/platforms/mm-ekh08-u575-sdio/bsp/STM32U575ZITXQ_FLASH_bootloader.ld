/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32U575xI Device from STM32U5 series
**                      2048Kbytes FLASH
**                      784Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2022 STMicroelectronics.
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
_estack = ORIGIN(RAM) + LENGTH(RAM);	/* end of "RAM" Ram type memory */

_Min_Heap_Size = 0x200 ;	/* required amount of heap  */
_Min_Stack_Size = 0x400 ;	/* required amount of stack */

__FILESYSTEM_SIZE = DEFINED(__FILESYSTEM_SIZE) ? __FILESYSTEM_SIZE : 0xE0000;

/* Memories definition */
MEMORY
{
  RAM           (xrw)   : ORIGIN = 0x20000000,  LENGTH = 768K
  SRAM4         (xrw)   : ORIGIN = 0x28000000,  LENGTH = 16K
  BOOTLOADER    (rx)    : ORIGIN = 0x08000000,  LENGTH = 16K
  CONFIG        (r)     : ORIGIN = 0x08004000,  LENGTH = 16K
  FLASH         (rx)    : ORIGIN = 0x08008000, LENGTH = (0x08200000 - 0x08008000) - __FILESYSTEM_SIZE
  FILESYSTEM    (r)     : ORIGIN = (0x08200000 - __FILESYSTEM_SIZE), LENGTH = __FILESYSTEM_SIZE
}

/* Sections */
SECTIONS
{
  .mmconfig :
  {
    mmconfig_start  = ORIGIN(CONFIG);
    mmconfig_end    = ORIGIN(CONFIG) + LENGTH(CONFIG);
  }

  .filesystem :
  {
    filesystem_start  = ORIGIN(FILESYSTEM);
    filesystem_end    = ORIGIN(FILESYSTEM) + LENGTH(FILESYSTEM);
  }

  .application :
  {
    application_start  = ORIGIN(FLASH);
    application_end    = ORIGIN(FLASH) + LENGTH(FLASH);
  }

  /* The program code and other data into "FLASH" Rom type memory */
  .text :
  {
    KEEP(*(.bootloader))   /* .bootloader sections (code) */
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    _etext = .;        /* define a global symbols at end of code */
  } >BOOTLOADER

  /* Constant data into "FLASH" Rom type memory */
  .rodata :
  {
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
  } >BOOTLOADER

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >BOOTLOADER

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >BOOTLOADER

  .preinit_array :
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

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> BOOTLOADER

  /* Uninitialized data section into "RAM" Ram type memory */
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* Uninitialized data section that is not zeroed at boot */
  .noinit :
  {
    *(.noinit)
    *(.noinit.*)
    . = ALIGN(4);
  } >RAM

  /* User_heap_stack section, used to check that there is enough "RAM" Ram type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    *(.isr_vector)
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
