set(TARGET_NAME PHAL_G474)
add_library(${TARGET_NAME} STATIC)

# Find all .c sources in project
file(GLOB_RECURSE glob_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
target_sources(${TARGET_NAME} PRIVATE ${glob_sources})

# Find directories for '#include'
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# phal_G4's own sources self-include root-relative (e.g. "common/phal_G4/adc/adc.h").
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} PUBLIC CMSIS_G474)
