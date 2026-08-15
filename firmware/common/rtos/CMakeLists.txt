MACRO(MAKE_FREERTOS_WRAPPER LIB_NAME FREERTOS_CORE)
    set(TARGET_NAME ${LIB_NAME})
    add_library(${TARGET_NAME} STATIC)

    # Only include local .c files for this wrapper if needed (optional)
    file(GLOB_RECURSE glob_sources "*.c")
    target_sources(${TARGET_NAME} PRIVATE ${glob_sources})

    # Includes (typically for board-specific glue code, if any)
    target_include_directories(${TARGET_NAME}
        PUBLIC ${CMAKE_SOURCE_DIR}
    )

    # Link to the shared FreeRTOS core for this board
    target_link_libraries(${TARGET_NAME}
        PUBLIC ${FREERTOS_CORE}
    )
ENDMACRO()

MAKE_FREERTOS_WRAPPER(FREERTOS_F407 FREERTOS_LIB_F407)
MAKE_FREERTOS_WRAPPER(FREERTOS_G474 FREERTOS_LIB_G474)
