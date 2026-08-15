add_firmware_component(
    NAME f4_testing
    LINKER_SCRIPT "STM32F407VGTx_FLASH"
    LIBS "CMSIS_F407;PHAL_F407;RTOS_F407;can_node_PDU"
)
