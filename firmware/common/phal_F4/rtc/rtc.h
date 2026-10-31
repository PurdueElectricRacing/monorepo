/**
 * @file rtc.h
 * @author Chris McGalliard (cmcgalli@purdue.edu)
 * @brief RTC Configuration Driver for STM32F4 Devices
 * @version 0.1
 * @date 2024-01-12
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef _PHAL_RTC_H_
#define _PHAL_RTC_H_

#include "common/phal_F4/phal_F4.h"

// Clock source: currently only supporting LSE

/*
    When both prescalers are used, it is recommended to configure the asynchronous
    prescaler to a high value to minimize consumption. (RM0430 RTC chapter)

    Default Configuration STM32F4
    =======================================================
    The asynchronous prescaler division factor is set to 128, and the synchronous division factor to 256,
    to obtain an internal clock frequency of 1 Hz (ck_spre) with an LSE frequency of 32.768 kHz.
*/

// Maximum input frequency is 4MHz, reccomended to use LSE at 32.768 kHz
// Minimum division factor: 1
// Maximum division factor: 2^22

// RTC Clock = source / ((ASCNC + 1) * (SYNC + 1))
// To target 1Hz on LSI, we are doing 32000 / ((255 + 1) * (124 + 1))
#define RTC_ASYNC_PRESCAL (255U)
#define RTC_SYNC_PRESCAL  (124U)

// STM32 RTC uses lovely BCD
// https://embedded.fm/blog/2018/6/5/an-introduction-to-bcd

typedef enum {
    RTC_MONTH_UNKNOWN   = 0x0,
    RTC_MONTH_JANUARY   = 0x1,
    RTC_MONTH_FEBRUARY  = 0x2,
    RTC_MONTH_MARCH     = 0x3,
    RTC_MONTH_APRIL     = 0x4,
    RTC_MONTH_MAY       = 0x5,
    RTC_MONTH_JUNE      = 0x6,
    RTC_MONTH_JULY      = 0x7,
    RTC_MONTH_AUGUST    = 0x8,
    RTC_MONTH_SEPTEMBER = 0x9,
    RTC_MONTH_OCTOBER   = 0x10,
    RTC_MONTH_NOVEMBER  = 0x11,
    RTC_MONTH_DECEMBER  = 0x12,
} RTC_MONTH_t;

typedef enum {
    RTC_WEEKDAY_UNKNOWN   = 0x0,
    RTC_WEEKDAY_MONDAY    = 0x1,
    RTC_WEEKDAY_TUESDAY   = 0x2,
    RTC_WEEKDAY_WEDNESDAY = 0x3,
    RTC_WEEKDAY_THURSDAY  = 0x4,
    RTC_WEEKDAY_FRIDAY    = 0x5,
    RTC_WEEKDAY_SATURDAY  = 0x6,
    RTC_WEEKDAY_SUNDAY    = 0x7,
} RTC_WEEKDAY_t;

typedef enum {
    RTC_FORMAT_24_HOUR = 0U,
    RTC_FORMAT_AM_PM,
} RTC_TIME_FORMAT_t;

typedef struct
{
    RTC_MONTH_t month_bcd;
    RTC_WEEKDAY_t weekday;
    uint8_t day_bcd;
    uint8_t year_bcd;

} RTC_date_t;

typedef struct
{
    //  Must be a number between 0x00 and 0x12 if RTC_FORMAT_AM_PM
    //  Must be a number between 0x00 and 0x23 if RTC_FORMAT_24_HOUR
    uint8_t hours_bcd;

    // Must be a number between 0x0 and 0x59
    uint8_t minutes_bcd;
    uint8_t seconds_bcd;

    RTC_TIME_FORMAT_t time_format;
} RTC_time_t;

typedef struct
{
    RTC_date_t date;
    RTC_time_t time;
} RTC_timestamp_t;

#define RTC_CONV_TO_BCD(v) (((v / 10) << 4) | (v % 10))

bool PHAL_getTimeRTC(RTC_timestamp_t* currentTimestamp);
uint8_t PHAL_configureRTC(RTC_timestamp_t* initial_time, bool force_time);

#endif // _PHAL_RTC_H_