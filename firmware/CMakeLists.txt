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

# Common Variables
set(PROJECT_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)
set(SUPPORT_DIR ${CMAKE_SOURCE_DIR}/support)

# Add Libraries
include(cmake/utils.cmake)
include(cmake/dependencies.cmake)
include(cmake/common_component.cmake) # Macro for defining the common build process
include(cmake/can_library.cmake)

set(COMMON_MODULES
    common/amk
    common/bangbang
    common/rtos
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

set(SOURCE_MODULES
    source/f4_testing
    source/g4_testing
    source/a_box
    source/dashboard
    source/daq
    source/driveline
    source/main_module
    source/pdu
    source/torque_vector
)

foreach(MODULE IN LISTS COMMON_MODULES EXTERNAL_MODULES SOURCE_MODULES)
    message(STATUS "Adding module: ${MODULE}")
    add_subdirectory(${MODULE})
endforeach()

# Don't hold external modules to -Werror=conversion
foreach(EXTERNAL_MODULE IN LISTS EXTERNAL_MODULES)
    file(GLOB_RECURSE EXTERNAL_MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${EXTERNAL_MODULE}/*.c")
    set_source_files_properties(${EXTERNAL_MODULE_SOURCES}
        DIRECTORY source/daq
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion"
    )
endforeach()
