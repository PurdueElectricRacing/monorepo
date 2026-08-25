# Helper for generating common CMake targets in the components directroy

# Archive one linked target and derive address-aware HEX plus contiguous BIN.
# objcopy's binary output starts at the lowest load address, fills internal
# gaps with erased-flash bytes, and therefore avoids an absolute-address prefix.
function(postbuild_target TARGET_NAME COMPONENT_NAME OUTPUT_DIR_OVERRIDE MAP_FILE)
    set(OUTPUT_FILE_NAME ${COMPONENT_NAME})

    # Archive address-aware HEX and contiguous BIN artifacts.
    if(OUTPUT_DIR_OVERRIDE)
        set(COMPONENT_OUTPUT_DIR ${OUTPUT_DIR_OVERRIDE})
    else()
        set(COMPONENT_OUTPUT_DIR ${PROJECT_OUTPUT_DIR}/${COMPONENT_NAME})
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPONENT_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${TARGET_NAME} ${COMPONENT_OUTPUT_DIR}/${OUTPUT_FILE_NAME}.elf
        COMMAND ${CMAKE_COMMAND} -E copy ${MAP_FILE} ${COMPONENT_OUTPUT_DIR}/${OUTPUT_FILE_NAME}.elf.map
        COMMENT "Archive target"
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -S -O ihex ${TARGET_NAME} ${COMPONENT_OUTPUT_DIR}/${OUTPUT_FILE_NAME}.hex
        COMMENT "Generating HEX file"
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -S -O binary --gap-fill 0xFF
                ${TARGET_NAME} ${COMPONENT_OUTPUT_DIR}/${OUTPUT_FILE_NAME}.bin
        COMMENT "Generating contiguous binary image"
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_SIZE_UTIL} ${TARGET_NAME} 
        COMMENT "Binary Output Size"
    )
endfunction()

# Helper to get CPU flags based on STM32 Family
function(get_cpu_flags FAMILY_NAME OUT_FLAGS)
    if(${FAMILY_NAME} MATCHES "STM32F4.*" OR ${FAMILY_NAME} MATCHES "STM32G4.*")
        set(${OUT_FLAGS} "-mcpu=cortex-m4" "-mfloat-abi=hard" "-mfpu=fpv4-sp-d16" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown STM32 Family: ${FAMILY_NAME}")
    endif()
endfunction()

MACRO(SUBDIRLIST curdir result)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

MACRO(RECURSE_DIRECTORIES curdir search_term return_list)
    FILE(GLOB_RECURSE new_list RELATIVE ${curdir} ${search_term})
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
ENDMACRO()
