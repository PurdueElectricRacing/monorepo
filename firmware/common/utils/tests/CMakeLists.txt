add_firmware_unit_test(
    NAME abs_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)