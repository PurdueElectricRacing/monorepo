/**
 * @file imd.h
 * @brief ABOX IMD PWM capture and CAN reporting

 * @author Aditya Saini (saini19@purdue.edu)
 */

#ifndef IMD_H
#define IMD_H

#include <stdint.h>

static constexpr uint32_t IMD_PERIOD_MS = 1000;

// Maximum count before overflow (655.36 ms at 100 kHz).
static constexpr uint32_t IMD_TIMER_ARR = 0xFFFFU;

static constexpr uint32_t IMD_CAPTURE_IRQ_PRIORITY = 6U;

static constexpr uint32_t IMD_TIMER_TICK_HZ = 100000U;
static constexpr uint32_t IMD_CAPTURE_TIMEOUT_MS = 250U;

// PWM frequencies and duty limits from the IR155(-3203) datasheet
static constexpr float IMD_NORMAL_FREQUENCY_HZ = 10.0;
static constexpr float IMD_UNDERVOLTAGE_FREQUENCY_HZ = 20.0;
static constexpr float IMD_STARTUP_FREQUENCY_HZ = 30.0;
static constexpr float IMD_DEVICE_ERROR_FREQUENCY_HZ = 40.0;
static constexpr float IMD_CONNECTION_FAULT_FREQUENCY_HZ = 50.0;
static constexpr float IMD_FREQUENCY_TOLERANCE_PCT = 5.0;
static constexpr float IMD_PERCENT_SCALE = 100.0;

static constexpr float IMD_MEASUREMENT_MIN_DUTY_PCT = 5.0;
static constexpr float IMD_MEASUREMENT_MAX_DUTY_PCT = 95.0;
static constexpr float IMD_STARTUP_GOOD_MAX_DUTY_PCT = 10.0;
static constexpr float IMD_STARTUP_BAD_MIN_DUTY_PCT = 90.0;
static constexpr float IMD_FAULT_MIN_DUTY_PCT = 47.5;
static constexpr float IMD_FAULT_MAX_DUTY_PCT = 52.5;

static constexpr float IMD_RESISTANCE_AT_HALF_DUTY_KOHM = 1200.0;
static constexpr float IMD_MAX_MEASURABLE_RESISTANCE_KOHM = 10000.0;

// Set up TIM5 CH2 to measure PWM input (IMD LS) on PC12.
void imd_init(void);

// Report the newest completed cycle
// The ISR continues acquiring at the input frequency
// The snapshot age is independent of reporting rate
void imd_periodic(void);

#endif // IMD_H
