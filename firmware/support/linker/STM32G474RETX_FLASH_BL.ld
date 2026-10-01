/*
******************************************************************************
**
** @file        : STM32G474_BL.ld
**
** @brief       : Linker script for STM32G474 resident CAN bootloader
**                - 512 KB FLASH (2 KB pages)
**                - 128 KB RAM
**
** Executable code occupies the first 16 KiB. The loadable image also includes
** 0xFF padding across the metadata region; programming tools must erase those
** pages before writing to clear stale metadata. The bootloader reads metadata
** and writes application bytes to these addresses:
**
**   0x08000000 – 0x08003FFF : bootloader (16 KiB)
**   0x08004000 – 0x08007FFF : metadata region (16 KiB)
**   0x08008000 – 0x0807FFFF : application slot (480 KiB)
**
** Application vector table starts at: 0x08008000
**
******************************************************************************
*/

ENTRY(Reset_Handler)

/* Stack top */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Optional heap/stack reservation */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

/* Application region pointer (boot jump target) */
PROVIDE(_eboot_flash = 0x08008000);

/* Memory regions */
MEMORY
{
  RAM      (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 16K  /* Executable bootloader only */
  METADATA (rx)  : ORIGIN = 0x08004000, LENGTH = 16K  /* Loadable erased-state padding */
}

/* Sections */
SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
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

  .ARM.exidx :
  {
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
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT>FLASH
  _sidata = LOADADDR(.data);

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
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

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  /* Include erased-state metadata padding in the load image for programming tools. */
  .metadata_padding :
  {
    FILL(0xFF)
    BYTE(0xFF)
    . = . + LENGTH(METADATA) - 1;
  } > METADATA

  .ARM.attributes 0 : { *(.ARM.attributes) }

  ASSERT(ADDR(.metadata_padding) == ORIGIN(METADATA),
         "metadata padding must start at 0x08004000")
  ASSERT(SIZEOF(.metadata_padding) == LENGTH(METADATA),
         "metadata padding must be exactly 16 KiB")
  ASSERT(LOADADDR(.metadata_padding) + SIZEOF(.metadata_padding) == ORIGIN(METADATA) + LENGTH(METADATA),
         "metadata padding must end at 0x08008000")

  /* Keep this resident image inside the first 16 KiB flash region. */
  ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(FLASH) + LENGTH(FLASH),
         "resident bootloader exceeds its 16 KiB flash region")
}
