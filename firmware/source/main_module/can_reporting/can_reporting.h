#ifndef CAN_REPORTING_H
#define CAN_REPORTING_H

#include <stdint.h>

static constexpr uint32_t CAN_REPORTING_PERIOD_MS = 5000;
void can_report_periodic(void);

#endif // CAN_REPORTING_H