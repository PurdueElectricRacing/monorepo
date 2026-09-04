#ifndef GEODETIC_H
#define GEODETIC_H

/**
 * @file geodetic.h
 * @brief Geodetic coordinate utility header.
 *
 * Projects GPS latitude/longitude onto a local cartesian plane so that planar
 * geometry can be used on it.
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include <math.h>
#include <stdint.h>

#include "common/utils/linear_algebra.h"
#include "common/utils/units.h"

// WGS84 mean meters of northing per degree of latitude
static constexpr double METERS_PER_DEGREE = 111132.0;

// u-blox NAV-PVT and the gps_coordinates CAN message both scale degrees by 1e-7
static constexpr double GEODETIC_DEGREE_SCALE = 1e-7;

typedef struct {
    double latitude_deg;
    double longitude_deg;
} geodetic_coord_t;

/**
 * @brief Convert a 1e-7 degree fixed point coordinate pair into degrees.
 *
 * @param latitude Latitude in 1e-7 degrees
 * @param longitude Longitude in 1e-7 degrees
 * @return The coordinate in degrees.
 */
static inline geodetic_coord_t geodetic_from_scaled(const int32_t latitude, const int32_t longitude) {
    return (geodetic_coord_t){
        .latitude_deg  = (double)latitude * GEODETIC_DEGREE_SCALE,
        .longitude_deg = (double)longitude * GEODETIC_DEGREE_SCALE,
    };
}

/**
 * @brief Project a coordinate onto a local east-north plane about an origin.
 *
 * Equirectangular approximation: northing is proportional to latitude, and
 * easting is compressed by the cosine of the origin latitude. Sub-meter
 * accurate over the few kilometers a track spans, and linear, so straight
 * lines stay straight and segment geometry remains valid.
 *
 * @param origin The coordinate that maps to (0, 0)
 * @param coord The coordinate to project
 * @return East (x) and north (y) offset from the origin, in meters.
 */
static inline vector2_t geodetic_to_local(const geodetic_coord_t origin, const geodetic_coord_t coord) {
    double latitude_delta_deg = coord.latitude_deg - origin.latitude_deg;
    double longitude_delta_deg = coord.longitude_deg - origin.longitude_deg;

    radians_t origin_latitude = radians_from((degrees_t){.value = (float)origin.latitude_deg});

    return (vector2_t){
        .x = (float)(longitude_delta_deg * METERS_PER_DEGREE) * cosf(origin_latitude.value),
        .y = (float)(latitude_delta_deg * METERS_PER_DEGREE),
    };
}

#endif // GEODETIC_H
