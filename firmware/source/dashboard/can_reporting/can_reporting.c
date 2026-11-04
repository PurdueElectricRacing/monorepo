#include "can_reporting.h"

#include "can_library/generated/DASHBOARD.h"

volatile can_stats_t can_stats;

void report_can_status(void) {
    CAN_SEND_can_overflow(can_stats.rx_overflow, can_stats.tx_overflow);
}