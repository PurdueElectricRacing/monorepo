add_firmware_unit_test(
    NAME strbuf_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../strbuf.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/strbuf_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/.."
)
