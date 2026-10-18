/**
 * @file imd.h
 * @brief ABOX IMD PWM measurement and state reporting
 *
 * @author Aditya Saini (saini19@purdue.edu)
 */

#ifndef IMD_H
#define IMD_H

#include <stdint.h>

#include "common/rtos/rtos.h"

static constexpr uint32_t IMD_PERIOD_MS = 1000;

// Maximum count before overflow (655.36 ms at 100 kHz).
static constexpr uint32_t IMD_TIMER_ARR = 0xFFFFU;

static constexpr uint32_t IMD_CAPTURE_IRQ_PRIORITY = 6U;
static_assert(IMD_CAPTURE_IRQ_PRIORITY > configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

// set up TIM1 to measure PWM input (IMD LS) on PA8
void imd_init(void);

// Report the newest completed cycle
// The ISR continues acquiring at the input frequency
// The snapshot age is independent of reporting rate
void imd_periodic(void);

#endif // IMD_H
