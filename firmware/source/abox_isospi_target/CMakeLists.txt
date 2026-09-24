add_firmware_component(
    NAME abox_isospi_target
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "CMSIS_G474;RTOS_G474;PHAL_G474;STRBUF;LERP_LUT"
)
