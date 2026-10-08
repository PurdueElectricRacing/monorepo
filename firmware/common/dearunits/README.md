# DearUnits

Generated typed wrappers around physical units. Each unit is a struct holding a single `float`, so they cost nothing at runtime but stop you from accidentally mixing, e.g., feet with meters.

Available unit families: temperature (C/F), distance (m/cm/mm/in/ft/mi), time (ms/s/min/hr/day), angle (rad/deg), mass (g/kg/lb), pressure (Pa/psi/bar), velocity (mps/kph/mph).

```c
#include "common/dearunits/generated/dearunits.h"

celsius_t board_temp     = { .value = 42.0f };
fahrenheit_t temperature = fahrenheit_from_celsius(board_temp); // 107.6 F

degree_t degrees = { .value = 90.0f };
radian_t radians = DU_RADIAN_FROM(degrees);

// in-class conversions
meter_t distance  = DU_METER_FROM((foot_t){ .value = 10.0f });
second_t time     = DU_SECOND_FROM((minute_t){ .value = 5.0f });

// typesafe arithmetic prevents mixing units (the following two lines wont compile)
meter_t invalid_sum     = DU_ADD(distance, time); // can't add distance and time
second_t invalid_result = DU_DIVIDE(distance, time); // velocity cannot be assigned to time

// compound quantities can be made from base units
meters_per_second_t speed      = velocity_from(distance, time);
meters_per_second_t same_speed = DU_DIVIDE(distance, time);
meter_t travelled              = DU_MULTIPLY(speed, time);
```

## Tests

Host tests use Google Test, with a C23 shim to exercise the generated `_Generic` macros. Run from the repository root:

```sh
cmake -S tests -B firmware/build/host-tests -DPER_TEST_COVERAGE=OFF
cmake --build firmware/build/host-tests --target dearunits_test
ctest --test-dir firmware/build/host-tests -R DearUnitsTest --output-on-failure
```

Generate `generated/dearunits.h` first with `python3 generators/generate.py` if it is missing.
