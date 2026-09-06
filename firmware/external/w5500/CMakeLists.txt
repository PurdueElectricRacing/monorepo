set(TARGET_NAME W5500)

file(GLOB_RECURSE SOURCES "*.c")

add_library(${TARGET_NAME} STATIC)

target_sources(${TARGET_NAME} PRIVATE ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Needed for FPU ABI flags (-mfloat-abi=hard etc.) to match consumers; see per_add_stm32_cmsis_target.
target_link_libraries(${TARGET_NAME} PRIVATE CMSIS_F407)

# third-party driver, not maintained for -Werror=conversion.
target_compile_options(${TARGET_NAME} PRIVATE "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion")
