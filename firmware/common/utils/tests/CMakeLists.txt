add_firmware_unit_test(
    NAME abs_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)

add_firmware_unit_test(
    NAME max_tests
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/max_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/max_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)

add_firmware_unit_test(
    NAME min_tests
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/min_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/min_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)