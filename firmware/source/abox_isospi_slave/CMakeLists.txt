add_firmware_component(
    NAME abox_isospi_slave
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "CMSIS_G474;RTOS_G474;PHAL_G474"
)

# Bound byte-interrupt latency even in the bench debug build.
target_compile_options(abox_isospi_slave.elf PRIVATE -O2)
