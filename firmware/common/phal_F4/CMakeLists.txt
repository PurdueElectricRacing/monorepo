set(TARGET_NAME PHAL_F407)
add_library(${TARGET_NAME} STATIC)

# Find all .c sources in project
file(GLOB_RECURSE glob_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
target_sources(${TARGET_NAME} PRIVATE ${glob_sources})

# Find directories for '#include'
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# phal_F4's own sources self-include root-relative (e.g. "common/phal_F4/adc/adc.h").
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} PUBLIC CMSIS_F407)

# hand-maintained HAL, held to the pre -Werror=conversion baseline for now.
target_compile_options(${TARGET_NAME} PRIVATE "-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion")
