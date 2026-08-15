# Host tests

The host test suite uses GoogleTest and CTest for firmware unit tests.

## Requirements

- Python 3.11 or newer
- CMake 3.21 or newer
- A C23 and C++17 host compiler
- GoogleTest installed locally and discoverable by CMake
- `gcov`, `lcov`, and `genhtml` on `PATH` (`sudo apt-get install gcc g++ lcov libgtest-dev` on Ubuntu or `brew install lcov googletest` on macOS)

Coverage requires a tool from the same compiler family as the host compiler,
because GCC and LLVM emit incompatible `.gcno`/`.gcda` files. The configure
step selects the coverage frontend from the configured C compiler and fails if
that frontend, `lcov`, or `genhtml` is unavailable.

- GCC host builds prefer `gcov-<major>` for the newer of the configured C and
  C++ compiler majors (for example, `gcov-16` for GCC 16), then fall back to
  `gcov`. The search also looks next to the C compiler, which handles layouts
  such as Homebrew's `gcc-16` and `gcov-16`.
- Clang/AppleClang builds use `llvm-cov gcov`, preferring a matching
  `llvm-cov-<major>` executable before falling back to `llvm-cov`.
- C and C++ compilers must both be GCC or Clang-family compilers. The selected
  frontend is based on the C compiler, so mixed compiler families should be
  avoided.
- On macOS `/usr/bin/gcc` is AppleClang in disguise. For GCC coverage builds,
  install real GCC (`brew install gcc`) and run with
  `CC=gcc-<version> CXX=g++-<version> python3 per_build.py tests` (e.g.
  `CC=gcc-16 CXX=g++-16`).

## Running the tests

Run tests through `per_build.py` from the repository root:

| Command | Action |
| --- | --- |
| `python3 per_build.py tests` | Build host tests with coverage and sanitizers |

Coverage and AddressSanitizer/UBSan are enabled by default for GNU, Clang, and
AppleClang builds. The coverage target resets counters, runs CTest, captures
gcov data with lcov, filters test and dependency sources, and writes the report
to `firmware/build/host-tests/coverage/html/index.html`.

`per_build.py` dispatches test commands to `tests/build_tests.py`, which
configures and builds the host tests and generates the coverage report.
Build artifacts are stored in `firmware/build/host-tests`.

The `host_tests.yml` GitHub Actions workflow runs the unit tests with coverage
on pull requests and pushes to `master`, and uploads the generated HTML report
as the `host-test-coverage` artifact.

The same steps can be run directly with CMake:

```sh
cmake -S tests -B firmware/build/host-tests \
  -DPER_TEST_SANITIZERS=ON \
  -DPER_TEST_COVERAGE=ON
cmake --build firmware/build/host-tests --target coverage
```

Both instrumentation modes are enabled by default.

## Existing tests

Unit tests live under `tests/unit/firmware`:

- `lerp_lut_test.cpp` covers exact lookup points, interpolation, and upper and lower clamping in `firmware/common/lerp_lut/lerp_lut.c`.
- `can_codec_test.cpp` covers payload loading and storage, byte swapping, signal packing and unpacking, sign extension, and float bit conversion in `firmware/can_library/can_codec.h`. A C23 shim ensures these header-only inline functions are compiled as C rather than as part of the C++17 GoogleTest translation unit.

`tests/cmake/FirmwareUnitTest.cmake` provides `add_firmware_unit_test`. It
configures production C sources as C23 static libraries, test sources as C++17,
strict compiler warnings, GoogleTest discovery, the CTest `unit` label, and
AddressSanitizer/UBSan and coverage instrumentation.

## Directory layout

```text
tests/
├── ARCHITECTURE.md
├── build_tests.py
├── CMakeLists.txt
├── cmake/
│   └── FirmwareUnitTest.cmake
└── unit/
    ├── CMakeLists.txt
    └── firmware/
        ├── can_codec_test.cpp
        └── lerp_lut_test.cpp
```

## Adding a unit test

1. Add a GoogleTest source file under `tests/unit/firmware`.
2. Register a target in `tests/unit/CMakeLists.txt` with `add_firmware_unit_test`:

   ```cmake
   add_firmware_unit_test(
       NAME example_test
       SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../firmware/common/example/example.c"
       TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/firmware/example_test.cpp"
       INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../firmware/common/example"
   )
   ```

3. Use `SOURCES` for production `.c` files. For header-only C modules, add a
   `.c` shim to `SOURCES` and call it from the C++ test so inline implementation
   code is compiled under C23 rather than C++17.
4. Run `python3 per_build.py tests`.

CTest discovers each GoogleTest case from the registered target automatically.
