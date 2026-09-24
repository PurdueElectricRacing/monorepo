/**
 * @file thermistor.h
 * @brief Thermistor lookup table from datasheet.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef THERMISTOR_H
#define THERMISTOR_H

#include "common/lerp_lut/lerp_lut.h"

extern const lerp_lut_t thermistor_lut;

// Convert a thermistor resistance to a temperature using the LUT
[[gnu::always_inline]]
static inline float thermistor_R_to_T(float resistance) {
    return lut_lookup(&thermistor_lut, resistance);
}

#endif // THERMISTOR_H