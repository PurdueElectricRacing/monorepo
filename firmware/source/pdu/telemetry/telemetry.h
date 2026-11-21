/**
 * @file telemetry.h
 * @brief PDU telemetry task implementations
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

static constexpr uint32_t TELEMETRY_02HZ_PERIOD_MS = 5000;
void report_telemetry_02hz(void);

#endif // TELEMETRY_H
