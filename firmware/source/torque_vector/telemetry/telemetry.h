#ifndef TELEMETRY_H
#define TELEMETRY_H

/**
 * @file telemetry.h
 * @brief TORQUE VECTOR telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

static constexpr uint32_t TELEMETRY_25HZ_PERIOD_MS = 40;
void report_telemetry_25hz(void);

static constexpr uint32_t TELEMETRY_1HZ_PERIOD_MS = 1000;
void report_telemetry_1hz(void);

static constexpr uint32_t TELEMETRY_02HZ_PERIOD_MS = 5000;
void report_telemetry_02hz(void);

#endif // TELEMETRY_H