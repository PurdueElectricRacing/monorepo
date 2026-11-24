/**
 * @file hv_soc.c
 * @brief HV pack state of charge estimation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef HV_SOC_H
#define HV_SOC_H

#include <stdint.h>

static constexpr uint32_t HV_SOC_PERIOD_MS = 10;

void hv_soc_init(void);
void hv_soc_periodic(void);


#endif // HV_SOC_H
