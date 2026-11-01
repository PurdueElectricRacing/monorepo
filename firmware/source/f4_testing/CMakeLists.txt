add_firmware_component(
    NAME f4_testing
    LINKER_SCRIPT "STM32F423RHTx_FLASH"
    LIBS "CMSIS_F423;PHAL_F423;RTOS_F423"
)