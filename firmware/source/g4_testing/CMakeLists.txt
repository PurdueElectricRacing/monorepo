set(G4_TESTING_TEST "TEST_BLINKY" CACHE STRING "G4 test to build, see g4_testing.h")

create_can_node_lib(G4_TESTING)

add_firmware_component(
    NAME g4_testing
    LINKER_SCRIPT "STM32G474RETX_FLASH"
    LIBS "can_node_G4_TESTING;CMSIS_G474;PHAL_G474;RTOS_G474"
    DEFINES "G4_TESTING_CHOSEN=${G4_TESTING_TEST}"
)

if(G4_TESTING_TEST STREQUAL "TEST_DIAGNOSTICS")
    target_link_libraries(g4_testing.elf PRIVATE DIAGNOSTICS)
endif()
