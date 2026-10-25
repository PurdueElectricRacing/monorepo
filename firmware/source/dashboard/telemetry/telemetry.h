#ifndef TELEMETRY_H
#define TELEMETRY_H

/**
 * @file telemetry.h
 * @brief DASHBOARD telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

static constexpr uint32_t TELEMETRY_02HZ_PERIOD_MS = 5000;
void report_telemetry_02hz(void);

#endif // TELEMETRY_H