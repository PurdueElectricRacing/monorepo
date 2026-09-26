add_firmware_unit_test(
    NAME bangbang_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../bangbang.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/bangbang_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/.."
)