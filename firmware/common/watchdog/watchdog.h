/**
 * @file watchdog.h
 * @brief Basic watchdog implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include "common/rtos/rtos.h"

void WDG_init(void);
void WDG_pet(void);
uint32_t WDG_get_CSR(void);
bool was_reset_by_WDG(void);

static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 1000;
static constexpr uint32_t WATCHDOG_PET_PERIOD_MS = 100;

#define DEFINE_WATCHDOG_TASK() FREERTOS_DEFINE_TASK(WDG_pet, WATCHDOG_PET_PERIOD_MS, TASK_PRIORITY_LOW, STACK_256)

#define START_WATCHDOG_TASK() FREERTOS_START_TASK(WDG_pet)

#endif // WATCHDOG_H