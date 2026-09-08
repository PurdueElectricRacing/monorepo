
set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER   arm-none-eabi-gcc     REQUIRED)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc     REQUIRED)
find_program(CMAKE_OBJCOPY      arm-none-eabi-objcopy REQUIRED)
find_program(CMAKE_SIZE_UTIL    arm-none-eabi-size    REQUIRED)

set(COMMON_FLAGS 
    "-mthumb"
    "--specs=nosys.specs"
)

set(C_FLAGS      
    "--specs=nano.specs"
    "-std=c23"
    "-ffunction-sections"
    "-fdata-sections"
    "-ffreestanding"
    "-fno-common"
    "-fno-math-errno"
    "-fdiagnostics-color=always"

    "-Og"
    "-g3"
    
    "-Wall"
    "-Werror"
    "-Wshadow"
    "-Wdouble-promotion"
    "-Wsign-compare"
    "-Wunused-result"
    "-Wformat=2"
    "-Wformat-truncation"
    "-Werror=conversion"

    "-fanalyzer"
    "-Wno-analyzer-infinite-loop"
)

set(LINKER_FLAGS 
    "-Wl,--gc-sections"
    "-Wl,--no-warn-rwx-segments"
    "-lm"
)

set(ASM_FLAGS
    "-x assembler-with-cpp"
)

# Convert lists back to strings for CMake initialization variables
string(JOIN " " COMMON_FLAGS_STR ${COMMON_FLAGS})
string(JOIN " " C_FLAGS_STR      ${C_FLAGS})
string(JOIN " " LINKER_FLAGS_STR ${LINKER_FLAGS})
string(JOIN " " ASM_FLAGS_STR    ${ASM_FLAGS})

set(CMAKE_C_FLAGS_INIT          "${COMMON_FLAGS_STR} ${C_FLAGS_STR}"   CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_INIT        "${COMMON_FLAGS_STR} ${ASM_FLAGS_STR}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${LINKER_FLAGS_STR}"                  CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_EXPORT_COMPILE_COMMANDS           ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES  ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS   ON)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE         ON)