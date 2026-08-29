/**
 * @file imu.c
 * @brief IMU calibration and coordinate transformations
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/generated/TORQUE_VECTOR.h"
#include "common/utils/orientation.h"
#include "sensors.h"
// #include <math.h>
#include "common/utils/units.h"
#include "common/utils/clamp.h"

typedef enum {
    DECOUPLING_STATE_IDLE,
    DECOUPLING_STATE_CALIBRATING,
    DECOUPLING_STATE_ACTIVE
} decoupling_state_t;

// static volatile decoupling_state_t decoupling_state = DECOUPLING_STATE_IDLE;
static matrix3x3_t mounting_offset_matrix = {0};
imu_data_t imu_data = {0};
// static matrix3x3_t calibration_matrix;

// Statistics for calibration
// static constexpr uint32_t CALIBRATION_SAMPLE_THRESHOLD = 100;
// static vector3_t accel_sum = {
//     .x = 0,
//     .y = 0,
//     .z = 0
// };
// static uint32_t num_samples = 0;

static_assert(IZZE_ANGULAR_RATE_LAYOUT_HASH == IMU_ANGULAR_RATE_LAYOUT_HASH,
    "IZZE_angular_rate and IMU_angular_rate layout should be identical"
);

static_assert(IZZE_ACCELERATION_LAYOUT_HASH == IMU_ACCELERATION_LAYOUT_HASH,
    "IZZE_acceleration and IMU_acceleration layout should be identical"
);

/**
 * @brief Initialize the IMU decoupling calibration data structures and state.
 */
void initialize_calibration(void) {
    // accel_sum.x      = 0;
    // accel_sum.y      = 0;
    // accel_sum.z      = 0;
    // num_samples      = 0;

    float pi_2 = PI_F / 2.0f;
    euler_angles_t mounting_offset = {2.2689280276f, 0, -pi_2};
    mounting_offset_matrix = DCM_from_euler(mounting_offset);

    // decoupling_state = DECOUPLING_STATE_CALIBRATING;
}

/**
 * @brief Finalize the IMU decoupling calibration by deriving the rotation matrix from the accumulated accelerometer data.
 */
// static void finalize_calibration(void) {
//     if (num_samples < CALIBRATION_SAMPLE_THRESHOLD) {
//         return;
//     }

//     vector3_t avg_accel = {
//         .x = accel_sum.x / num_samples,
//         .y = accel_sum.y / num_samples,
//         .z = accel_sum.z / num_samples
//     };

//     vector3_t unit_g = vector3_normalize(avg_accel);

//     // Target coordinate frame: X+ is forward, Y+ is left, Z+ is up
//     // unit_g should be [0, 0, -1]
//     euler_angles_t angles = {0};

    // avoid asinf() domain errors due to fp noise by clamping the input to [-0.9999, 0.9999]
    // float clamped_pitch = CLAMP(-unit_g.x, -0.9999f, 0.9999f);


//     angles.pitch = asinf(clamped_pitch); // Pitch is rotation around Y: sin(pitch) = -accel_x
//     angles.roll  = atan2f(-unit_g.y, -unit_g.z); // Roll is rotation around X: sin(roll) = accel_y / cos(pitch)
//     angles.yaw   = 0.0f; // Yaw cannot be determined by gravity alone

//     calibration_matrix = tait_bryan(angles);
//     decoupling_state = DECOUPLING_STATE_ACTIVE;
// }

/**
 * @brief Callback function for handling IZZE angular rate data.
 *
 * In active mode, applies the decoupling rotation to the raw angular rate data and sends it out as IMU angular rate.
 */
void IZZE_angular_rate_CALLBACK(void) {
    // if (decoupling_state != DECOUPLING_STATE_ACTIVE) {
    //     return;
    // }

    vector3_t raw_data = {
        .x = can_data.IZZE_angular_rate.X_axis * UNPACK_COEFF_IZZE_ANGULAR_RATE_X_AXIS,
        .y = can_data.IZZE_angular_rate.Y_axis * UNPACK_COEFF_IZZE_ANGULAR_RATE_Y_AXIS,
        .z = can_data.IZZE_angular_rate.Z_axis * UNPACK_COEFF_IZZE_ANGULAR_RATE_Z_AXIS
    };

    // raw_data = matrix_multiply_vector3(&mounting_offset_matrix, &raw_data);

    vector3_t calibrated_data = matrix_multiply_vector3(&mounting_offset_matrix, &raw_data);
    imu_data.gyro_x = calibrated_data.x;
    imu_data.gyro_y = calibrated_data.y;
    imu_data.gyro_z = calibrated_data.z;

    CAN_SEND_IMU_angular_rate(
        (int16_t)(imu_data.gyro_x * PACK_COEFF_IMU_ANGULAR_RATE_X_AXIS),
        (int16_t)(imu_data.gyro_y * PACK_COEFF_IMU_ANGULAR_RATE_Y_AXIS),
        (int16_t)(imu_data.gyro_z * PACK_COEFF_IMU_ANGULAR_RATE_Z_AXIS)
    );
}

/**
 * @brief Callback function for handling IZZE acceleration data.
 *
 * In calibration mode, accumulates the raw acceleration data to derive the calibration rotation.
 * In active mode, applies the decoupling rotation to the raw acceleration data and sends it out as IMU acceleration.
 */
void IZZE_acceleration_CALLBACK(void) {
    vector3_t raw_data = {
        .x = can_data.IZZE_acceleration.X_axis * UNPACK_COEFF_IZZE_ACCELERATION_X_AXIS,
        .y = can_data.IZZE_acceleration.Y_axis * UNPACK_COEFF_IZZE_ACCELERATION_Y_AXIS,
        .z = can_data.IZZE_acceleration.Z_axis * UNPACK_COEFF_IZZE_ACCELERATION_Z_AXIS
    };

    // raw_data = matrix_multiply_vector3(&mounting_offset_matrix, &raw_data);

    uint16_t temperature = (uint16_t)(can_data.IZZE_acceleration.temperature * UNPACK_COEFF_IZZE_ACCELERATION_TEMPERATURE);

    // if (decoupling_state == DECOUPLING_STATE_CALIBRATING) {
    //     accel_sum.x += raw_data.x;
    //     accel_sum.y += raw_data.y;
    //     accel_sum.z += raw_data.z;
    //     num_samples++;

    //     if (num_samples >= CALIBRATION_SAMPLE_THRESHOLD) {
    //         finalize_calibration();
    //     }
    // }

    // if (decoupling_state != DECOUPLING_STATE_ACTIVE) {
    //     return;
    // }

    vector3_t calibrated_data = matrix_multiply_vector3(&mounting_offset_matrix, &raw_data);
    imu_data.accel_x = calibrated_data.x;
    imu_data.accel_y = calibrated_data.y;
    imu_data.accel_z = calibrated_data.z;

    CAN_SEND_IMU_acceleration(
        (int16_t)(imu_data.accel_x * PACK_COEFF_IMU_ACCELERATION_X_AXIS),
        (int16_t)(imu_data.accel_y * PACK_COEFF_IMU_ACCELERATION_Y_AXIS),
        (int16_t)(imu_data.accel_z * PACK_COEFF_IMU_ACCELERATION_Z_AXIS),
        (int16_t)(temperature * PACK_COEFF_IMU_ACCELERATION_TEMPERATURE)
    );
}
