add_firmware_unit_test(
    NAME abs_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/abs_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)

add_firmware_unit_test(
    NAME clamp_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/clamp_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/clamp_test.cpp"
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

add_firmware_unit_test(
    NAME vector2_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/vector2_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/vector2_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)

add_firmware_unit_test(
    NAME orientation_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/orientation_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/orientation_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
        "${CMAKE_CURRENT_LIST_DIR}/../../.."
)

add_firmware_unit_test(
    NAME rescale_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/rescale_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/rescale_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)