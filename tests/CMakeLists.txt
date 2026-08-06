cmake_minimum_required(VERSION 3.21)
project(firmware_host_tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

get_filename_component(PER_TEST_FIRMWARE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../firmware" REALPATH)

option(PER_TEST_SANITIZERS "Build host tests with AddressSanitizer and UBSan" ON)
option(PER_TEST_COVERAGE "Build host unit tests with gcov coverage" ON)
if(PER_TEST_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang" OR
       NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(FATAL_ERROR "PER_TEST_COVERAGE requires a GNU, Clang, or AppleClang C and C++ compiler")
    endif()
    set(PER_TEST_GCOV_NAMES gcov)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" PER_TEST_GCC_MAJOR "${CMAKE_C_COMPILER_VERSION}")
        if(PER_TEST_GCC_MAJOR)
            list(PREPEND PER_TEST_GCOV_NAMES "gcov-${PER_TEST_GCC_MAJOR}")
        endif()
    endif()
    find_program(PER_TEST_GCOV NAMES ${PER_TEST_GCOV_NAMES})
    find_program(PER_TEST_LCOV lcov)
    find_program(PER_TEST_GENHTML genhtml)
    if(NOT PER_TEST_GCOV OR NOT PER_TEST_LCOV OR NOT PER_TEST_GENHTML)
        message(FATAL_ERROR
            "PER_TEST_COVERAGE requires gcov, lcov, and genhtml (install lcov and ensure all are on PATH)")
    endif()
endif()

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)
add_subdirectory(unit)
if(PER_TEST_COVERAGE)
    get_property(PER_TEST_TARGETS GLOBAL PROPERTY PER_TEST_TARGETS)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage"
        COMMAND "${PER_TEST_LCOV}" --directory "${CMAKE_BINARY_DIR}" --zerocounters
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
        COMMAND "${PER_TEST_LCOV}" --directory "${CMAKE_BINARY_DIR}" --gcov-tool "${PER_TEST_GCOV}"
            --capture
            --include "${PER_TEST_FIRMWARE_SOURCE_DIR}/*"
            --exclude "*/tests/*"
            --output-file "${CMAKE_BINARY_DIR}/coverage/lcov.info"
        COMMAND "${PER_TEST_GENHTML}" "${CMAKE_BINARY_DIR}/coverage/lcov.info"
            --output-directory "${CMAKE_BINARY_DIR}/coverage/html"
        DEPENDS ${PER_TEST_TARGETS}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
        COMMENT "Run unit tests and generate coverage report"
        VERBATIM
    )
endif()
