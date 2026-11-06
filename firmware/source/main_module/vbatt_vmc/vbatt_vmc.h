#ifndef VBATT_VMC_H
#define VBATT_VMC_H

/**
 * @file vbatt_vmc.h
 * @brief MAIN MODULE vbatt and vmc reading
 */

#include <stdbool.h>

// Both return false if acquisition has stopped (DMA error); *voltage_v is untouched.
bool vbatt_get_voltage(float *voltage_v);

bool vmc_get_voltage(float *voltage_v);

#endif // VBATT_VMC_H
