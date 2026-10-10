#ifndef COLORS_H
#define COLORS_H

/**
 * @file colors.h
 * @brief Color definitions for LCD display
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

// PER26 Color Pallette in 565 format
static constexpr uint16_t WHITE        = 65535; // #FFFFFF
static constexpr uint16_t DARK_GRAY    = 33808; // #838383
static constexpr uint16_t LIGHT_GRAY   = 57051; // #D9D9D9
static constexpr uint16_t BLUE         = 991;   // #007AFF
static constexpr uint16_t MUTED_BLUE   = 2278;  // #0B1D31
static constexpr uint16_t GREEN        = 1632;  // #00CC00
static constexpr uint16_t MUTED_GREEN  = 2369;  // #0B2A0B
static constexpr uint16_t BLACK        = 0;     // #000000
static constexpr uint16_t RED          = 63878; // #FF3336
static constexpr uint16_t MUTED_RED    = 12418; // #311313
static constexpr uint16_t YELLOW       = 65252; // #FFDE21
static constexpr uint16_t MUTED_YELLOW = 12642; // #312C10

/**
 * @brief determine the color of the text based on celcius temperature
 *
 * @param value current temperature value in celcius
 * @param warning_threshold warning temperature threshold in celcius
 * @param critical_threshold critical temperature threshold in celcius
 * @return uint16_t color value
 */
static inline uint16_t determine_temp_display_color(uint16_t value, uint16_t warning_threshold, uint16_t critical_threshold) {
    if (value >= critical_threshold) {
        return RED;
    } else if (value >= warning_threshold) {
        return YELLOW;
    } else {
        return WHITE;
    }
}

#endif // COLORS_H