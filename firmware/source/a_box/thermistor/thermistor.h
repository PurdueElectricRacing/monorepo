/**
 * @file thermistor.h
 * @brief Cell and onboard thermistor lookup tables from datasheets
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef THERMISTOR_H
#define THERMISTOR_H

#include "common/lerp_lut/lerp_lut.h"

extern const lerp_lut_t cell_thermistor_lut;
extern const lerp_lut_t board_thermistor_lut;

// Convert a cell thermistor resistance to temperature (Celsius)
[[gnu::always_inline]]
static inline float cell_thermistor_R_to_T(float resistance) {
    return lut_lookup(&cell_thermistor_lut, resistance);
}

// Convert an onboard thermistor resistance to temperature (Celsius)
[[gnu::always_inline]]
static inline float board_thermistor_R_to_T(float resistance) {
    return lut_lookup(&board_thermistor_lut, resistance);
}

#endif // THERMISTOR_H
