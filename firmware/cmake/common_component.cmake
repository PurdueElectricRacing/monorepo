
# Function to add a firmware component
# Usage:
# add_firmware_component(
#     NAME <name>
#     LINKER_SCRIPT <script_base_name>
#     LIBS <list_of_libs>
#     [IS_BOOTLOADER]
# )
#
# Bootloader additions: IS_BOOTLOADER selects the resident _BL.ld layout;
# BOOTLOADER_BUILD selects _APP.ld, defines BOOTLOADER_ENABLED, and requires the
# application to start at 0x08008000.
function(add_firmware_component)
    set(options IS_BOOTLOADER)
    set(oneValueArgs NAME LINKER_SCRIPT OUTPUT_DIR)
    set(multiValueArgs LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TARGET_NAME ${ARG_NAME}.elf)
    add_executable(${TARGET_NAME})

    # Link common libs
    target_link_libraries(${TARGET_NAME} PRIVATE SYSCALLS ${ARG_LIBS})

    # Sources: include all .c files in current directory and subdirectories
    file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
    list(FILTER SOURCES EXCLUDE REGEX "test_.*|.*starter.*")
    target_sources(${TARGET_NAME} PRIVATE ${SOURCES})

    # Includes: current directory and all subdirectories
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
    )
    RECURSE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR} "*.h" INCLUDE_DIRS)
    foreach(DIR ${INCLUDE_DIRS})
        target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${DIR})
    endforeach()

    # Select resident, bootloader-aware application, or standalone layout.
    if(BOOTLOADER_BUILD AND NOT ARG_IS_BOOTLOADER)
        set(LS_SUFFIX "_APP.ld")
        target_compile_definitions(${TARGET_NAME} PRIVATE BOOTLOADER_ENABLED=1)
    elseif(ARG_IS_BOOTLOADER)
        set(LS_SUFFIX "_BL.ld")
        target_compile_definitions(${TARGET_NAME} PRIVATE BOOTLOADER_FIRMWARE=1)
    else()
        set(LS_SUFFIX ".ld")
    endif()
    
    set(MAP_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.map)
    target_link_options(${TARGET_NAME} PRIVATE
        -T${SUPPORT_DIR}/linker/${ARG_LINKER_SCRIPT}${LS_SUFFIX}
        "-Wl,-Map=${MAP_FILE}"
    )

    # Post-build actions
    postbuild_target(
        ${TARGET_NAME}
        ${ARG_NAME}
        "${ARG_OUTPUT_DIR}"
        ${MAP_FILE}
    )
endfunction()
