cmake_minimum_required(VERSION 3.31 FATAL_ERROR)

# Check for bootloader generation
if (NOT BOOTLOADER_BUILD)
    message(STATUS "Not building applications for bootloaders.")
else()
    message(STATUS
    "Building applications and bootloaders! "
    "Your application will NOT work if you are NOT using the bootloader download tool.") # You have been warned
endif()

# Setup arm-none-eabi as our compile toolcahin
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchain.cmake)

# Setup project
project(PER_FIRMWARE VERSION 26.0 LANGUAGES C ASM)

# Archive to new outut directory
set(PROJECT_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)

# Common Variables
set(COMMON_SOURCE_DIR ${CMAKE_SOURCE_DIR}/common)
set(SUPPORT_DIR ${CMAKE_SOURCE_DIR}/support)

# Add Libraries
include(cmake/utils.cmake)
include(cmake/FindCMSIS.cmake)
include(cmake/FindSTM32HAL.cmake)

# See cmake/FindCMSIS.cmake and cmake/FindSTM32HAL.cmake for constructing additional libraries
make_cmsis_library(CMSIS_F407 STM32F4xx STM32F407xx ${CMAKE_SOURCE_DIR}/external/STM32CubeF4/Drivers/CMSIS)
make_cmsis_library(CMSIS_G474 STM32G4xx STM32G474xx ${CMAKE_SOURCE_DIR}/external/STM32CubeG4/Drivers/CMSIS)

include(cmake/FindFreeRTOS.cmake)

# Include new CAN Library
include(can_library/can_library.cmake)

set(COMMON_MODULES
    common/amk
    common/bootloader
    common/bangbang
    common/freertos
    common/heartbeat
    common/lerp_lut
    common/nextion
    common/phal_F4
    common/phal_G4
    common/sdio
    common/strbuf
    common/syscalls
    common/ublox
    common/watchdog
)

set(EXTERNAL_MODULES
    external/w5500
    external/fatfs
)

foreach(MODULE IN LISTS COMMON_MODULES)
    add_subdirectory(${MODULE})
endforeach()

foreach(MODULE IN LISTS EXTERNAL_MODULES)
    add_subdirectory(${MODULE})
endforeach()

# Macro for defining the common build process across all firmware components
include(cmake/common_component.cmake)

# Make MODULES overridable from the command line
set(MODULES "" CACHE STRING "Semicolon-separated list of source modules to build")

if(MODULES STREQUAL "")
    set(MODULES
        f4_testing
        g4_testing
        a_box
        dashboard
        daq
        driveline
        main_module
        pdu
        torque_vector
    )
endif()

# Loop over and add source modules
foreach(MODULE ${MODULES})
    message(STATUS "Adding module: ${MODULE}")
    add_subdirectory(source/${MODULE})
endforeach()
