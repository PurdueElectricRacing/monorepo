add_firmware_unit_test(
    NAME dearunits_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/dearunits_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/dearunits_test.cpp"
    INCLUDE_DIRECTORIES "${PER_TEST_FIRMWARE_SOURCE_DIR}" "${CMAKE_CURRENT_LIST_DIR}"
)
