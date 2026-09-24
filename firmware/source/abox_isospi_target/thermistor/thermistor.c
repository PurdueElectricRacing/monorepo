/**
 * @file thermistor.c
 * @brief Thermistor lookup table from datasheet.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/lerp_lut/lerp_lut.h"

// https://www.tdk-electronics.tdk.com/inf/50/db/ntc/NTC_Mini_sensors_S861.pdf
// from pg 6: R/T No. 8016
static constexpr size_t B57861S0103A039_LUT_SIZE = 43;
// Resistance (Ohms) -> temperature (Celsius)
const lut_entry_t B57861S0103A039_data[B57861S0103A039_LUT_SIZE] = {
    {.key = 165.3f, .value = 155.0f },
    {.key = 185.3f, .value = 150.0f },
    {.key = 208.3f, .value = 145.0f },
    {.key = 234.8f, .value = 140.0f },
    {.key = 265.4f, .value = 135.0f },
    {.key = 300.9f, .value = 130.0f },
    {.key = 341.7f, .value = 125.0f },
    {.key = 389.3f, .value = 120.0f },
    {.key = 445.4f, .value = 115.0f },
    {.key = 511.2f, .value = 110.0f },
    {.key = 588.6f, .value = 105.0f },
    {.key = 680.0f, .value = 100.0f },
    {.key = 788.5f, .value = 95.0f },
    {.key = 917.7f, .value = 90.0f },
    {.key = 1072.0f, .value = 85.0f },
    {.key = 1258.0f, .value = 80.0f },
    {.key = 1481.0f, .value = 75.0f },
    {.key = 1752.0f, .value = 70.0f },
    {.key = 2083.0f, .value = 65.0f },
    {.key = 2488.0f, .value = 60.0f },
    {.key = 2986.0f, .value = 55.0f },
    {.key = 3603.0f, .value = 50.0f },
    {.key = 4369.0f, .value = 45.0f },
    {.key = 5327.0f, .value = 40.0f },
    {.key = 6531.0f, .value = 35.0f },
    {.key = 8057.0f, .value = 30.0f },
    {.key = 10'000.0f, .value = 25.0f },
    {.key = 12'490.0f, .value = 20.0f },
    {.key = 15'710.0f, .value = 15.0f },
    {.key = 19'900.0f, .value = 10.0f },
    {.key = 25'390.0f, .value = 5.0f },
    {.key = 32'650.0f, .value = 0.0f },
    {.key = 42'320.0f, .value = -5.0f },
    {.key = 55'330.0f, .value = -10.0f },
    {.key = 72'930.0f, .value = -15.0f },
    {.key = 97'070.0f, .value = -20.0f },
    {.key = 130'400.0f, .value = -25.0f },
    {.key = 177'000.0f, .value = -30.0f },
    {.key = 242'600.0f, .value = -35.0f },
    {.key = 336'500.0f, .value = -40.0f },
    {.key = 471'700.0f, .value = -45.0f },
    {.key = 670'100.0f, .value = -50.0f },
    {.key = 963'000.0f, .value = -55.0f }
};

LERP_LUT_INIT(thermistor_lut, B57861S0103A039_data, B57861S0103A039_LUT_SIZE);
