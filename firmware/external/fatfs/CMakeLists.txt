set(TARGET_NAME FATFS)

file(GLOB_RECURSE SOURCES "*.c")

add_library(${TARGET_NAME} STATIC)

target_sources(${TARGET_NAME} PRIVATE ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# diskio.c is daq-specific glue (RTC/SDIO HAL + daq's main.h), not generic FatFS.
# Scoped PRIVATE so this coupling doesn't leak to other consumers of FATFS.
# (Firmware root comes transitively from RTOS_F407, which PUBLIC-exposes it.)
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/daq
    ${CMAKE_SOURCE_DIR}/source/daq/sd_card
    ${CMAKE_SOURCE_DIR}/source/daq/spmc
)
target_link_libraries(${TARGET_NAME} PRIVATE PHAL_F407 RTOS_F407 SDIO)

# third-party library, not maintained for -Werror=conversion.
target_compile_options(${TARGET_NAME} PRIVATE "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion")
