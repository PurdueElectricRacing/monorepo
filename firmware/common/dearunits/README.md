# DearUnits

Generated typed wrappers around physical units. Each unit is a struct holding a single `float`, so they cost nothing at runtime but stop you from accidentally mixing, e.g., feet with meters.

Available unit families: temperature (C/F), distance (m/cm/mm/in/ft/mi), time (ms/s/min/hr/day), angle (rad/deg), mass (g/kg/lb), pressure (Pa/psi/bar), velocity (mps/kph/mph).

```c
#include "common/dearunits/generated/dear_units.h"

celsius_t board_temp = { .value = 42.0f };
fahrenheit_t f       = fahrenheit_from_celsius(board_temp); // 107.6 F

degree_t steer_deg = { .value = 90.0f };
radian_t steer_rad = DU_RADIAN_FROM(steer_deg); // pi/2

// _Generic shorthand: the right converter is picked from the input type.
meter_t  d = DU_METER_FROM((foot_t){ .value = 10.0f }); // 3.048 m
second_t t = DU_SECOND_FROM((minute_t){ .value = 5.0f }); // 300 s
```
