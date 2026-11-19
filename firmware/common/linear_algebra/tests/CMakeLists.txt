add_firmware_unit_test(
    NAME linear_algebra_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../vector3.c" "${CMAKE_CURRENT_LIST_DIR}/../matrix3.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/vector3_test.cpp" "${CMAKE_CURRENT_LIST_DIR}/matrix3_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/.."
)
