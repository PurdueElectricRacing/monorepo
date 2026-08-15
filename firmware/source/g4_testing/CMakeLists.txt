create_can_node_lib(G4_TESTING "CMSIS_G474;RTOS_G474")

add_firmware_component(
    NAME g4_testing
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "can_node_G4_TESTING;CMSIS_G474;PHAL_G474;RTOS_G474"
)
