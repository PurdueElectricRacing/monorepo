#ifndef SENSORS_H
#define SENSORS_H

/**
 * @file sensors.h
 * @brief TV Managed sensor header
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

#include "ublox/nav_pvt.h"
#include "ublox/nav_relposned.h"

typedef struct {
    float gyro_y;
    float gyro_x;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;
} imu_data_t;

static constexpr size_t ROVER_RX_BUFFER_SIZE = NAV_PVT_BUFFER_SIZE + NAV_RELPOSNED_BUFFER_SIZE;
extern volatile uint8_t rover_rx_buffer[ROVER_RX_BUFFER_SIZE]; // DMA target
extern NAV_PVT_payload_t nav_pvt;
extern NAV_RELPOSNED_payload_t nav_relposned;
extern imu_data_t imu_data;

static constexpr uint32_t GPS_THREAD_PERIOD_MS = 100;
void gps_periodic(void);

// async imu decoupling handled by RX callbacks
void initialize_calibration(void);
void IZZE_angular_rate_CALLBACK(void);
void IZZE_acceleration_CALLBACK(void);

#endif // SENSORS_H
