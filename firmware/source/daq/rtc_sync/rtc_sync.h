/**
 * @file rtc_sync.h
 * @brief Synchronization of the RTC peripheral with the GPS UTC time
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Shriya Balu (balu@purdue.edu)
 */

#ifndef RTC_SYNC_H
#define RTC_SYNC_H

#include "timestamped_frame.h"
#include "common/phal_F4/rtc/rtc.h"
#include "common/rtos/rtos.h"

extern volatile uint32_t last_RTC_sync_time;
extern QueueHandle_t gps_time_queue;
extern RTC_timestamp_t fallback_timestamp;

void RTC_sync_init(void);
void RTC_sync(void);

#endif // RTC_SYNC_h