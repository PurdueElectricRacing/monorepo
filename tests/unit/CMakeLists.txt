include("${CMAKE_CURRENT_LIST_DIR}/../cmake/FirmwareUnitTest.cmake")

add_firmware_unit_test(
    NAME lerp_lut_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../firmware/common/lerp_lut/lerp_lut.c"
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/firmware/lerp_lut_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../firmware/common/lerp_lut"
)

add_firmware_unit_test(
    NAME can_codec_test
    TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/firmware/can_codec_test.cpp"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../firmware/can_library"
)
