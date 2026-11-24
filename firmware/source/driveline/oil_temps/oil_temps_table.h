/**
 * @file oil_temps_table.h
 * @brief Oil temp lookup table from datasheet.
 *
 * @author Anya Pokrovskaya (apokrovs@purdue.edu)
 */

#ifndef OIL_TEMPS_H
#define OIL_TEMPS_H

#include "lerp_lut/lerp_lut.h"

extern const lerp_lut_t oil_temps_lut;

[[gnu::always_inline]]
static inline float oil_temps_R_to_T(float resistance) {
    return lut_lookup(&oil_temps_lut, resistance);
}

#endif // OIL_TEMPS_H