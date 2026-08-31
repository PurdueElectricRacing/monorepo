include(../../can_library/can_library.cmake)
create_can_node_lib(DAQ "CMSIS_F407;RTOS_F407")

add_firmware_component(
    NAME daq
    LINKER_SCRIPT "STM32F407VGTx_FLASH"
    LIBS "can_node_DAQ;CMSIS_F407;PHAL_F407;RTOS_F407;SDIO;W5500;FATFS;HEARTBEAT;WATCHDOG"
)
