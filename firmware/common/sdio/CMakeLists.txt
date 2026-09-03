set(TARGET_NAME SDIO)
add_library(${TARGET_NAME} STATIC)

target_sources(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdio.c")

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# sdio.c needs daq specific files 
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/daq
    ${CMAKE_SOURCE_DIR}/source/daq/sd_card
    ${CMAKE_SOURCE_DIR}/source/daq/spmc
)
target_link_libraries(${TARGET_NAME} PRIVATE PHAL_F407 RTOS_F407)

# hand-maintained HAL, held to the pre -Werror=conversion baseline for now.
target_compile_options(${TARGET_NAME} PRIVATE "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion")
