create_can_node_lib(G4_TESTING "CMSIS_G474;RTOS_G474")

# The bootloader smoke-test image deliberately reuses the existing MAIN_MODULE
# bootloader identity so DaqApp can update it without a new protocol or target.
if(BOOTLOADER_BUILD)
    # Reuse the production MAIN_MODULE API: it includes main_version and the
    # callback-enabled bl_main_module_start message used by DaqApp.
    create_can_node_lib(MAIN_MODULE "CMSIS_G474;RTOS_G474")
    set(G4_TESTING_CAN_NODE can_node_MAIN_MODULE)
else()
    set(G4_TESTING_CAN_NODE can_node_G4_TESTING)
endif()

add_firmware_component(
    NAME g4_testing
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "${G4_TESTING_CAN_NODE};CMSIS_G474;PHAL_G474;RTOS_G474;BOOTLOADER_COMMON"
)
