add_firmware_unit_test(
    NAME can_codec_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/can_codec_test_shim.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/can_codec_test.cpp"
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/.."
)
