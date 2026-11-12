#include "can_reporting.h"

#include "can_library/generated/A_BOX.h"

extern volatile can_stats_t can_stats;

void can_report_periodic(void) {
    CAN_SEND_a_box_can_overflow(can_stats.rx_overflow, can_stats.tx_overflow);
    CAN_SEND_a_box_can_tx_actions(can_stats.tx_enqueue_count, can_stats.tx_task_wake_count);
    CAN_SEND_a_box_can_tx_messages(can_stats.tx_sent_count, can_stats.tx_callback_count);
}