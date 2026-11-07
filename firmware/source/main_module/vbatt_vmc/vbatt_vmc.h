#ifndef VBATT_VMC_H
#define VBATT_VMC_H

/**
 * @file vbatt_vmc.h
 * @brief MAIN MODULE vbatt and vmc reading
 */

// Returns the sensed HV voltage in volts.
float vbatt_get_voltage(void);

float vmc_get_voltage(void);

#endif // VBATT_VMC_H
