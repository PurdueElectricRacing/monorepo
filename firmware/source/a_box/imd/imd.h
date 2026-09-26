/**
 * @file imd.h
 * @brief ABOX IMD PWM measurement and state reporting
 *
 * @author Aditya Saini (saini19@purdue.edu)
 */

#ifndef IMD_H
#define IMD_H

#include <stdbool.h>
#include <stdint.h>

static constexpr uint32_t IMD_PERIOD_MS = 1000;

// timer ticks this many times per sec
static constexpr uint32_t IMD_TIMER_TICK_HZ = 100000; 
// max count before overflow
static constexpr uint32_t IMD_TIMER_ARR     = 0xFFFFU;

static constexpr uint8_t IMD_FREQ_TOLERANCE_HZ   = 3; 
static constexpr uint8_t IMD_FREQ_NORMAL_HZ      = 10;
static constexpr uint8_t IMD_FREQ_UNDERVOLTAGE_HZ = 20;
static constexpr uint8_t IMD_FREQ_SPEED_START_HZ = 30;
static constexpr uint8_t IMD_FREQ_DEVICE_ERROR_HZ = 40;
static constexpr uint8_t IMD_FREQ_CONN_FAULT_HZ  = 50;

void imd_init(void);

void imd_periodic(void);

#endif // IMD_H
