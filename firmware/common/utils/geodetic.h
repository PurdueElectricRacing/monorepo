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

#include "common/utils/linear_algebra.h"
#include "common/utils/units.h"

// WGS84 mean meters of northing per degree of latitude
static constexpr double METERS_PER_DEGREE = 111132.0;

typedef struct {
    double latitude_deg;
    double longitude_deg;
} geodetic_coord_t;

/**
 * @brief Build a coordinate from latitude and longitude already in degrees.
 *
 * Callers must convert packed CAN / GPS fixed-point values into degrees
 * before calling (for example with UNPACK_COEFF_GPS_COORDINATES_LATITUDE).
 *
 * @param latitude Latitude in degrees
 * @param longitude Longitude in degrees
 * @return The coordinate in degrees.
 */
static inline geodetic_coord_t geodetic_from_scaled(const float latitude, const float longitude) {
    return (geodetic_coord_t){
        .latitude_deg  = latitude,
        .longitude_deg = longitude,
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
    float latitude_delta_deg = coord.latitude_deg - origin.latitude_deg;
    float longitude_delta_deg = coord.longitude_deg - origin.longitude_deg;

    radians_t origin_latitude = radians_from((degrees_t){.value = (float)origin.latitude_deg});

    return (vector2_t){
        .x = (float)(longitude_delta_deg * METERS_PER_DEGREE) * cosf(origin_latitude.value),
        .y = (float)(latitude_delta_deg * METERS_PER_DEGREE),
    };
}

#endif // GEODETIC_H
