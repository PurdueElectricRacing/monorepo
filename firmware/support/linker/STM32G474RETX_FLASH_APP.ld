/*
 * Linker script for a bootloader-aware STM32G474 application (160 KiB).
 *
 * The vector table is intentionally placed at 0x08008000. The resident
 * bootloader validates that table and the image CRC before setting VTOR/MSP
 * and calling the application's reset handler.
 *
 *   0x08000000 - 0x08003FFF : resident bootloader
 *   0x08004000 - 0x08007FFF : metadata / reserved space
 *   0x08008000 - 0x0802FFFF : application image (this file)
 *
 * Do not use this layout without the matching bootloader image. For a normal
 * standalone G4 image, use STM32G474RETX_FLASH.ld instead.
 */

ENTRY(Reset_Handler)

/* Define memory regions */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 160K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Define stack and heap sizes */
_Min_Heap_Size = 0x200;  /* 512 B heap */
_Min_Stack_Size = 0x400; /* 1 KB stack */

_estack = ORIGIN(RAM) + LENGTH(RAM); /* top of stack */

/* Sections layout */
SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Vector table */
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)           /* code */
    *(.text*)          /* code */
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM.exidx : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT>FLASH
  _sidata = LOADADDR(.data);

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  /* Retain the one-shot bootloader request without a startup initializer. */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    _snoinit = .;
    *(.noinit*)
    . = ALIGN(4);
    _enoinit = .;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ : { libc.a ( * ) libm.a ( * ) libgcc.a ( * ) }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
