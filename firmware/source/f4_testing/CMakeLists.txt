add_firmware_component(
    NAME f4_testing
    LINKER_SCRIPT "STM32F423RHTx_FLASH"
    LIBS "CMSIS_F423;PHAL_F423;RTOS_F423"
)

# The PWM example starts FreeRTOS without referencing an RTOS wrapper task.
# Keep its static-allocation hooks in the link even when no wrapper is used.
target_link_options(f4_testing.elf PRIVATE "LINKER:--undefined=vApplicationGetIdleTaskMemory")
