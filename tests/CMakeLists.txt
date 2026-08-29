cmake_minimum_required(VERSION 3.21)

project(firmware_host_tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

get_filename_component(
    PER_TEST_FIRMWARE_SOURCE_DIR
    "${PROJECT_SOURCE_DIR}/../firmware"
    REALPATH
)

option(
    PER_TEST_SANITIZERS
    "Build host tests with AddressSanitizer and UBSan"
    ON
)

option(
    PER_TEST_COVERAGE
    "Build host unit tests with gcov coverage"
    ON
)

if(PER_TEST_COVERAGE)
    # Coverage is supported for GCC and Clang-family compilers. Select the
    # frontend that belongs to the C compiler; lcov supplies any wrapper args.
    if(
        NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
        OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
    )
        message(
            FATAL_ERROR
            "PER_TEST_COVERAGE requires a GNU, Clang, or AppleClang C and C++ compiler"
        )
    endif()

    # Extract compiler major versions for versioned gcov/llvm-cov lookup.
    string(
        REGEX MATCH "^[0-9]+"
        PER_TEST_C_MAJOR
        "${CMAKE_C_COMPILER_VERSION}"
    )
    string(
        REGEX MATCH "^[0-9]+"
        PER_TEST_CXX_MAJOR
        "${CMAKE_CXX_COMPILER_VERSION}"
    )

    # Also search next to the selected compiler, which handles installations
    # such as Homebrew where gcc-16 and gcov-16 live in the same directory.
    get_filename_component(
        PER_TEST_CC_DIR
        "${CMAKE_C_COMPILER}"
        DIRECTORY
    )

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # A newer gcov can read files produced by an older GCC, so select the
        # newer configured GCC major when C and C++ differ.
        set(PER_TEST_GCOV_MAJOR "${PER_TEST_C_MAJOR}")

        if(
            PER_TEST_CXX_MAJOR
            AND (
                NOT PER_TEST_GCOV_MAJOR
                OR PER_TEST_CXX_MAJOR GREATER PER_TEST_GCOV_MAJOR
            )
        )
            set(PER_TEST_GCOV_MAJOR "${PER_TEST_CXX_MAJOR}")
        endif()

        set(PER_TEST_GCOV_NAMES gcov)

        if(PER_TEST_GCOV_MAJOR)
            list(
                PREPEND PER_TEST_GCOV_NAMES
                "gcov-${PER_TEST_GCOV_MAJOR}"
            )
        endif()

        find_program(
            PER_TEST_GCOV
            NAMES ${PER_TEST_GCOV_NAMES}
            HINTS "${PER_TEST_CC_DIR}"
        )

        set(PER_TEST_GCOV_ARGS)
        set(PER_TEST_GCOV_FAMILY GNU)
    else()
        # Clang emits LLVM coverage data; use llvm-cov's gcov-compatible mode.
        set(PER_TEST_LLVM_COV_NAMES llvm-cov)

        if(PER_TEST_C_MAJOR)
            list(
                PREPEND PER_TEST_LLVM_COV_NAMES
                "llvm-cov-${PER_TEST_C_MAJOR}"
            )
        endif()

        find_program(
            PER_TEST_GCOV
            NAMES ${PER_TEST_LLVM_COV_NAMES}
            HINTS "${PER_TEST_CC_DIR}"
        )

        set(PER_TEST_GCOV_ARGS gcov)
        set(PER_TEST_GCOV_FAMILY LLVM)
    endif()

    find_program(PER_TEST_LCOV lcov)
    find_program(PER_TEST_GENHTML genhtml)

    if(NOT PER_TEST_GCOV OR NOT PER_TEST_LCOV OR NOT PER_TEST_GENHTML)
        message(
            FATAL_ERROR
            "PER_TEST_COVERAGE requires a compatible coverage tool, lcov, and "
            "genhtml (install lcov and ensure all are on PATH)"
        )
    endif()

    message(
        STATUS
        "PER_TEST_COVERAGE: using '${PER_TEST_GCOV}' (${PER_TEST_GCOV_FAMILY})"
    )
endif()

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)
add_subdirectory(unit)

if(PER_TEST_COVERAGE)
    get_property(
        PER_TEST_TARGETS
        GLOBAL
        PROPERTY PER_TEST_TARGETS
    )

    # llvm-cov must be invoked as `llvm-cov gcov`.
    if(PER_TEST_GCOV_ARGS)
        set(
            PER_TEST_GCOV_TOOL_ARGS
            --gcov-tool "${PER_TEST_GCOV_ARGS}"
        )
    endif()

    add_custom_target(
        coverage

        # Create the coverage output directory.
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/coverage"

        # Clear any existing coverage counters.
        COMMAND
            "${PER_TEST_LCOV}"
            --directory "${CMAKE_BINARY_DIR}"
            --zerocounters

        # Run the host test suite.
        COMMAND
            ${CMAKE_CTEST_COMMAND}
            --test-dir "${CMAKE_BINARY_DIR}"
            --output-on-failure

        # Capture coverage for firmware sources only.
        COMMAND
            "${PER_TEST_LCOV}"
            --directory "${CMAKE_BINARY_DIR}"
            --gcov-tool "${PER_TEST_GCOV}"
            ${PER_TEST_GCOV_TOOL_ARGS}
            --capture
            --include "${PER_TEST_FIRMWARE_SOURCE_DIR}/*"
            --exclude "*/tests/*"
            --output-file "${CMAKE_BINARY_DIR}/coverage/lcov.info"

        # Generate the HTML coverage report.
        COMMAND
            "${PER_TEST_GENHTML}"
            "${CMAKE_BINARY_DIR}/coverage/lcov.info"
            --output-directory "${CMAKE_BINARY_DIR}/coverage/html"

        DEPENDS
            ${PER_TEST_TARGETS}

        WORKING_DIRECTORY
            "${CMAKE_BINARY_DIR}"

        USES_TERMINAL
        COMMENT "Run unit tests and generate coverage report"
        VERBATIM
    )
endif()
