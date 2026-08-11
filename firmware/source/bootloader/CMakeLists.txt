# One image per G4 board; APP_ID selects its IDs and pins.
set(G4_BOOTLOADER_NODES
    MAIN_MODULE
    DASHBOARD
    A_BOX
    TORQUE_VECTOR
    FRONT_DRIVELINE
    REAR_DRIVELINE
)

foreach(NODE IN LISTS G4_BOOTLOADER_NODES)
    add_firmware_component(
        NAME bootloader_${NODE}
        LINKER_SCRIPT "STM32G474RETX_FLASH"
        LIBS "CMSIS_G474;PHAL_G474;BOOTLOADER_COMMON"
        IS_BOOTLOADER
    )

    target_compile_definitions(bootloader_${NODE}.elf PRIVATE APP_ID=APP_${NODE})
endforeach()

add_custom_target(bootloader.elf DEPENDS
    bootloader_MAIN_MODULE.elf
    bootloader_DASHBOARD.elf
    bootloader_A_BOX.elf
    bootloader_TORQUE_VECTOR.elf
    bootloader_FRONT_DRIVELINE.elf
    bootloader_REAR_DRIVELINE.elf
)
