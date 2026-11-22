# One image per G4 board; APP_ID selects its IDs and pins.
set(G4_BOOTLOADER_NODES
    main_module
    dashboard
    a_box
    torque_vector
    front_driveline
    rear_driveline
    pdu
)

foreach(NODE IN LISTS G4_BOOTLOADER_NODES)
    string(TOUPPER ${NODE} APP_NODE)
    add_firmware_component(
        NAME bootloader_${NODE}
        LINKER_SCRIPT "STM32G474RETX_FLASH"
        LIBS "CMSIS_G474;PHAL_G474;BOOTLOADER_COMMON"
        IS_BOOTLOADER
    )

    target_compile_definitions(bootloader_${NODE}.elf PRIVATE APP_ID=APP_${APP_NODE})
endforeach()

add_custom_target(bootloader.elf DEPENDS
    bootloader_main_module.elf
    bootloader_dashboard.elf
    bootloader_a_box.elf
    bootloader_torque_vector.elf
    bootloader_front_driveline.elf
    bootloader_rear_driveline.elf
    bootloader_pdu.elf
)
