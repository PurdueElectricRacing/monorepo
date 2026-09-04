set(TARGET_NAME FATFS)

file(GLOB_RECURSE SOURCES "*.c")

add_library(${TARGET_NAME} STATIC)

target_sources(${TARGET_NAME} PRIVATE ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# diskio.c access to sd_task_handle for SDIO ISR notifications + SD activity LED
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/daq
)
target_link_libraries(${TARGET_NAME} PRIVATE PHAL_F407 RTOS_F407 SDIO)

# third-party library, not maintained for -Werror=conversion.
target_compile_options(${TARGET_NAME} PRIVATE "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion")
