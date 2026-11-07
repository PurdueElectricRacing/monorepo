/**
 * @file board_thermistor.c
 * @brief Board thermistor lookup table from datasheet.
 *
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "thermistor.h"

// https://pages.hmc.edu/spjut/RocketE80/PDFs/MurataThermistor.pdf (Murata Cat. No. R44E.pdf)
// from pg 16 (catalog pg 15): Part Number NCP/NCU XH103 (10 kOhm, B = 3380 K)
static constexpr size_t NCU18XH103F60RB_LUT_SIZE = 34;
// Resistance (Ohms) -> temperature (Celsius)
const lut_entry_t NCU18XH103F60RB_data[NCU18XH103F60RB_LUT_SIZE] = {
    {.key = 531.0f, .value = 125.0f },
    {.key = 596.0f, .value = 120.0f },
    {.key = 672.0f, .value = 115.0f },
    {.key = 758.0f, .value = 110.0f },
    {.key = 858.0f, .value = 105.0f },
    {.key = 974.0f, .value = 100.0f },
    {.key = 1110.0f, .value = 95.0f },
    {.key = 1268.0f, .value = 90.0f },
    {.key = 1452.0f, .value = 85.0f },
    {.key = 1669.0f, .value = 80.0f },
    {.key = 1925.0f, .value = 75.0f },
    {.key = 2228.0f, .value = 70.0f },
    {.key = 2586.0f, .value = 65.0f },
    {.key = 3014.0f, .value = 60.0f },
    {.key = 3535.0f, .value = 55.0f },
    {.key = 4161.0f, .value = 50.0f },
    {.key = 4917.0f, .value = 45.0f },
    {.key = 5834.0f, .value = 40.0f },
    {.key = 6948.0f, .value = 35.0f },
    {.key = 8315.0f, .value = 30.0f },
    {.key = 10'000.0f, .value = 25.0f },
    {.key = 12'081.0f, .value = 20.0f },
    {.key = 14'674.0f, .value = 15.0f },
    {.key = 17'926.0f, .value = 10.0f },
    {.key = 22'021.0f, .value = 5.0f },
    {.key = 27'219.0f, .value = 0.0f },
    {.key = 33'892.0f, .value = -5.0f },
    {.key = 42'506.0f, .value = -10.0f },
    {.key = 53'650.0f, .value = -15.0f },
    {.key = 68'237.0f, .value = -20.0f },
    {.key = 87'559.0f, .value = -25.0f },
    {.key = 113'347.0f, .value = -30.0f },
    {.key = 148'171.0f, .value = -35.0f },
    {.key = 195'652.0f, .value = -40.0f }
};

LERP_LUT_INIT(board_thermistor_lut, NCU18XH103F60RB_data, NCU18XH103F60RB_LUT_SIZE);
