# DearUnits

Generated typed wrappers around physical units. Each unit is a struct holding a single `float`, so they cost nothing at runtime but stop you from accidentally mixing, e.g., feet with meters.

Available unit families: temperature (C/F), distance (m/cm/mm/in/ft/mi), time (ms/s/min/hr/day), angle (rad/deg), mass (g/kg/lb), pressure (Pa/psi/bar), velocity (mps/kph/mph).

```c
#include "common/dearunits/generated/dearunits.h"

celsius_t board_temp = { .value = 42.0f };
fahrenheit_t f       = fahrenheit_from_celsius(board_temp); // 107.6 F

degree_t steer_deg = { .value = 90.0f };
radian_t steer_rad = DU_RADIAN_FROM(steer_deg); // pi/2

// _Generic shorthand: the right converter is picked from the input type.
meter_t  d = DU_METER_FROM((foot_t){ .value = 10.0f }); // 3.048 m
second_t t = DU_SECOND_FROM((minute_t){ .value = 5.0f }); // 300 s
```

## Tests

Host tests use Google Test, with a C23 shim to exercise the generated `_Generic` macros. Run from the repository root:

```sh
cmake -S tests -B firmware/build/host-tests -DPER_TEST_COVERAGE=OFF
cmake --build firmware/build/host-tests --target dearunits_test
ctest --test-dir firmware/build/host-tests -R DearUnitsTest --output-on-failure
```

Generate `generated/dearunits.h` first with `python3 generators/generate.py` if it is missing.
