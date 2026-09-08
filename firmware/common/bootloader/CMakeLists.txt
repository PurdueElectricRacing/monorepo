# Shared by resident bootloaders and bootloader-aware applications.
add_library(BOOTLOADER_COMMON INTERFACE)

target_sources(BOOTLOADER_COMMON INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader_common.c
)

target_include_directories(BOOTLOADER_COMMON INTERFACE
    ${CMAKE_SOURCE_DIR}
)
