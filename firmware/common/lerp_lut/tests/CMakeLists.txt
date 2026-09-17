add_firmware_unit_test(
    NAME lerp_lut_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../lerp_lut.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/lerp_lut_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/.."
)
