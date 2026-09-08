create_can_node_lib(DRIVELINE "CMSIS_G474;RTOS_G474")

foreach(SIDE front rear)
    add_firmware_component(
        NAME ${SIDE}_driveline
        LINKER_SCRIPT "STM32G474RETX_FLASH"
        LIBS "can_node_DRIVELINE;CMSIS_G474;RTOS_G474;PHAL_G474;BOOTLOADER_COMMON;HEARTBEAT;LERP_LUT;WATCHDOG"
    )

    # Pass the location define to the compiler
    string(TOUPPER ${SIDE} SIDE_UPPER)
    target_compile_definitions(${SIDE}_driveline.elf PRIVATE IS_${SIDE_UPPER}_DRIVELINE)
endforeach()

# Group both driveline images under one CMake target.
add_custom_target(driveline.elf DEPENDS front_driveline.elf rear_driveline.elf)
