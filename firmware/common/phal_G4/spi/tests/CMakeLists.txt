add_firmware_unit_test(
    NAME spi_baud_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../spi_baud.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/spi_baud_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/.."
)
