# Third-party firmware dependency setup (CMSIS Core, CMSIS device headers, FreeRTOS kernel)

set(PER_CMSIS_CORE_DIR "${CMAKE_SOURCE_DIR}/external/CMSIS_6/CMSIS/Core/Include")
set(PER_FREERTOS_DIR "${CMAKE_SOURCE_DIR}/external/FreeRTOS-Kernel")

function(per_require_path path description)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR
            "Missing ${description}: ${path}\n"
            "Initialize the firmware submodules before configuring the build.")
    endif()
endfunction()

function(per_add_stm32_cmsis_target target family device device_dir)
    string(TOLOWER "${family}" family_lower)
    string(TOLOWER "${device}" device_lower)

    per_require_path("${PER_CMSIS_CORE_DIR}/core_cm4.h" "CMSIS Core")
    per_require_path("${device_dir}/Include/${device_lower}.h" "${device} CMSIS device headers")
    per_require_path("${device_dir}/Source/Templates/system_${family_lower}.c" "${family} system source")
    per_require_path("${device_dir}/Source/Templates/gcc/startup_${device_lower}.s" "${device} startup source")

    add_library(${target} STATIC
        "${device_dir}/Source/Templates/system_${family_lower}.c"
        "${device_dir}/Source/Templates/gcc/startup_${device_lower}.s"
    )

    get_cpu_flags("${family}" cpu_flags)
    target_compile_options(${target} PUBLIC ${cpu_flags})
    target_link_options(${target} INTERFACE ${cpu_flags})
    target_compile_options(${target} PRIVATE -fno-analyzer)

    target_compile_definitions(${target} PUBLIC ${family} ${device})
    target_include_directories(${target} PUBLIC
        "${PER_CMSIS_CORE_DIR}"
        "${device_dir}/Include"
    )
endfunction()

function(per_add_freertos_target target cmsis_target)
    per_require_path("${PER_FREERTOS_DIR}/include/FreeRTOS.h" "FreeRTOS Kernel")
    per_require_path("${PER_FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c" "FreeRTOS Cortex-M4F port")

    add_library(${target} STATIC
        "${PER_FREERTOS_DIR}/list.c"
        "${PER_FREERTOS_DIR}/queue.c"
        "${PER_FREERTOS_DIR}/tasks.c"
        "${PER_FREERTOS_DIR}/timers.c"
        "${PER_FREERTOS_DIR}/event_groups.c"
        "${PER_FREERTOS_DIR}/stream_buffer.c"
        "${PER_FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c"
    )

    target_include_directories(${target} PUBLIC
        "${PER_FREERTOS_DIR}/include"
        "${PER_FREERTOS_DIR}/portable/GCC/ARM_CM4F"
        "${CMAKE_SOURCE_DIR}/common/freertos"
    )
    target_link_libraries(${target} PUBLIC ${cmsis_target})
    target_compile_options(${target} PRIVATE -fno-analyzer)
endfunction()

per_add_stm32_cmsis_target(
    CMSIS_F407 STM32F4xx STM32F407xx
    "${CMAKE_SOURCE_DIR}/external/cmsis-device-f4"
)
per_add_stm32_cmsis_target(
    CMSIS_G474 STM32G4xx STM32G474xx
    "${CMAKE_SOURCE_DIR}/external/cmsis-device-g4"
)

per_add_freertos_target(FREERTOS_LIB_F407 CMSIS_F407)
per_add_freertos_target(FREERTOS_LIB_G474 CMSIS_G474)
