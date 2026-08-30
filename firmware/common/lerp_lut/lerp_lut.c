/**
 * @file lerp_lut.c
 * @brief Linear interpolation lookup table (LUT).
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "lerp_lut.h"

/**
 * @brief Lookup a value in a linear interpolation lookup table (LUT).
 *
 * This function performs a binary search to find the appropriate interval
 * in the LUT and then linearly interpolates between the two nearest entries.
 *
 * @param lut Pointer to the LUT.
 * @param key The key to look up.
 * @return The interpolated value.
 */
float lut_lookup(const lerp_lut_t* lut, float key) {
    const lut_entry_t *entries = lut->entries;
    const size_t size             = lut->size;

    // "out of bounds" keys get clamped to the nearest value
    if (key <= entries[0].key) { return entries[0].value; }
    if (key >= entries[size - 1].key) { return entries[size - 1].value; }

    // binary search for the correct interval
    size_t low = 1;
    size_t high = size - 1;
    while (low < high) {
        size_t mid = low + (high - low) / 2;

        if (key < entries[mid].key) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    // keys[i] <= key < keys[i + 1]
    size_t i = low - 1;

    // lerp between values[i] and values[i + 1]
    float t = (key - entries[i].key) / (entries[i + 1].key - entries[i].key);
    return entries[i].value + (entries[i + 1].value - entries[i].value) * t;
}
