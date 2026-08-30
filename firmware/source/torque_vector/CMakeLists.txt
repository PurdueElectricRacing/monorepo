create_can_node_lib(TORQUE_VECTOR "CMSIS_G474;RTOS_G474")

add_firmware_component(
    NAME torque_vector
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "can_node_TORQUE_VECTOR;CMSIS_G474;RTOS_G474;PHAL_G474;HEARTBEAT;UBLOX;WATCHDOG"
)

# vcu/* is generated from Matlab/Simulink and isn't hand-maintained for conversion warnings.
file(GLOB VCU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vcu/*.c")
set_source_files_properties(${VCU_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-error=conversion;-Wno-error=float-conversion")

