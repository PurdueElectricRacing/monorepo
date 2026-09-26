/**
 * @file telemetry.h
 * @brief ABOX Telemetry task implementations
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

static constexpr uint32_t BMS_PERIOD_MS = 200;

static constexpr uint32_t TELEMETRY_100HZ_PERIOD_MS = 10;
void report_telemetry_100hz(void);

static constexpr uint32_t TELEMETRY_8HZ_PERIOD_MS = 125;
void report_telemetry_8hz(void);

static constexpr uint32_t TELEMETRY_02HZ_PERIOD_MS = 5000;
void report_telemetry_02hz(void);

#endif // TELEMETRY_H