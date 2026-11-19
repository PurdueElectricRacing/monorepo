# PER Utility Functions

Header-only helpers for embedded C. All macros are type-safe (`_Generic` dispatched) and force-inlined when applicable. Include them directly:

```c
#include "common/utils/max.h"
#include "common/utils/clamp.h"
// ...
```

## `max.h` / `min.h`
Inline functions to compute the maximum or minimum of 2, 3, or 4 ints or floats.

```c
int a = 5, b = 3, c = 10, d = 7;
int hi = MAXOF(a, b);        // 5
hi = MAXOF(a, b, c);         // 10
hi = MAXOF(a, b, c, d);      // 10

float x = 1.5f, y = 2.3f, z = 0.8f;
float lo = MINOF(x, y);      // 1.5
lo = MINOF(x, y, z);         // 0.8
```

## `abs.h`
`ABS(x)` returns the absolute value of an int or float.

```c
int   ai = ABS(-7);     // 7
float af = ABS(-1.25f); // 1.25
```

## `clamp.h`
`CLAMP(input, lower, upper)` saturates a value into `[lower, upper]`. Works on int, unsigned int, long, unsigned long, and float.

```c
int duty       = CLAMP(raw_duty, 0, 100);            // 0..100 %
float throttle = CLAMP(pedal_pos, 0.0f, 1.0f);       // 0..1
```

## `rescale.h`
`RESCALE(input, in_min, in_max, out_min, out_max)` linearly maps a value from one range to another. Returns `out_min` if the input range is zero (no div-by-zero).

```c
// 12-bit ADC counts -> 0..3.3 V
float v = RESCALE((float)adc_counts, 0.0f, 4095.0f, 0.0f, 3.3f);

// Steering sensor counts -> degrees
int deg = RESCALE(counts, 0, 1023, -180, 180);
```

## `countof.h`
`countof(array)` returns the number of elements in a stack/global array. Passing a pointer is rejected at compile time.

```c
int rails[] = { RAIL_24V, RAIL_5V, RAIL_3V3 };
for (size_t i = 0; i < countof(rails); ++i) {
    enable_rail(rails[i]);
}
```

## `vector2.h`

Provides `vector2_t` and 2D vector arithmetic, dot and cross products, magnitude, normalization, distance, midpoint, and perpendicular operations.

## `orientation.h`

`DCM_from_euler` converts `euler_angles_t` angles in radians to a `mat3_t` direction cosine matrix using the ZYX rotation sequence.
