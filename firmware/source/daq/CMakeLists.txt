add_firmware_component(
    NAME daq
    LINKER_SCRIPT "STM32F407VGTx_FLASH"
    LIBS "CMSIS_F407;PHAL_F407;RTOS_F407;SDIO;W5500;FATFS;HEARTBEAT;WATCHDOG"
)

# don't hold third party libraries to -Werror=conversion.
foreach(EXTERNAL_MODULE IN LISTS EXTERNAL_MODULES)
    file(GLOB_RECURSE EXTERNAL_MODULE_SOURCES "${CMAKE_SOURCE_DIR}/${EXTERNAL_MODULE}/*.c")
    set_source_files_properties(${EXTERNAL_MODULE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion"
    )
endforeach()
