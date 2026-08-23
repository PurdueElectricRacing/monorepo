#ifndef ORIENTATION_H
#define ORIENTATION_H

#include "linear_algebra.h"

// ! SAE J670 Z-up convention: X+ forward, Y+ left, Z+ up
// ! Pitch is rotation around Y, Roll is rotation around X, Yaw is rotation around Z

typedef struct {
    float roll;
    float pitch;
    float yaw;
} euler_angles_t;

/**
 * @brief Convert Euler angles to a direction cosine matrix
 *
 * @param angles The Euler angles in radians
 * @return a 3x3 DCM representing the rotation
 */
static inline matrix3x3_t DCM_from_euler(const euler_angles_t angles) {
    float cos_roll  = cosf(angles.roll);
    float sin_roll  = sinf(angles.roll);
    float cos_pitch = cosf(angles.pitch);
    float sin_pitch = sinf(angles.pitch);
    float cos_yaw   = cosf(angles.yaw);
    float sin_yaw   = sinf(angles.yaw);

    // ZYX Euler sequence (Yaw -> Pitch -> Roll)
    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    matrix3x3_t dcm;
    dcm.data[0][0] = cos_pitch * cos_yaw;
    dcm.data[0][1] = cos_yaw * sin_pitch * sin_roll - cos_roll * sin_yaw;
    dcm.data[0][2] = sin_roll * sin_yaw + cos_roll * cos_yaw * sin_pitch;

    dcm.data[1][0] = cos_pitch * sin_yaw;
    dcm.data[1][1] = cos_roll * cos_yaw + sin_roll * sin_pitch * sin_yaw;
    dcm.data[1][2] = cos_roll * sin_pitch * sin_yaw - cos_yaw * sin_roll;

    dcm.data[2][0] = -sin_pitch;
    dcm.data[2][1] = cos_pitch * sin_roll;
    dcm.data[2][2] = cos_pitch * cos_roll;

    return dcm;
}

#endif // ORIENTATION_H