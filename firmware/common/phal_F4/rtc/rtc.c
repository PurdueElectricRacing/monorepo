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

#include "common/phal_F4/rtc/rtc.h"

// Registers are in line 10400 of the stm32f407xx.h file
uint8_t PHAL_configureRTC(RTC_timestamp_t* initial_time, bool force_time) {
    // Enable the LSI always (CSR is reset unlike the BDCR register)
    RCC->CSR |= RCC_CSR_LSION;
    // TODO: make timeout
    while (!(RCC->CSR & RCC_CSR_LSIRDY))
        ;

    // Check if already initialized
    if (!force_time && RTC->ISR & RTC_ISR_INITS)
        return true;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    // Once the RTCCLK clock source has been selected, the only possible way of modifying the selection is to reset the power domain
    // After system reset, the RTC registers are protected against parasitic write access
    //  with the DBP bit of the PWR power control register (PWR_CR)
    // The DBP bit must be set to enable RTC registers write access
#if defined(STM32F407xx)
    PWR->CR |= PWR_CR_DBP;
    while ((PWR->CR & PWR_CR_DBP) == 0)
        ;
#else
#error "Please define a MCU arch"
#endif

    // The LSEON, LSEBYP, RTCSEL and RTCEN bits in the RCC Backup domain control register (RCC_BDCR)
    // TODO: make propper timeout
    //  are in the Backup domain. As a result, after Reset, these bits are write-protected and the DBP bit
    //  has to be set before these can be modified.

    // Software reset backup power domain
    // RCC->BDCR |= RCC_BDCR_BDRST;

    RCC->BDCR &= ~(RCC_BDCR_RTCSEL); // Clear RTCSEL bits
    RCC->BDCR |= RCC_BDCR_RTCSEL_1; // select LSI
    RCC->BDCR |= RCC_BDCR_RTCEN;

    // After backup domain reset, all the RTC registers are write-protected
    // Writing to the RTC registers is enabled by writing a key into the Write Protection register, RTC_WPR

    // TODO, investigate backup domain reset conditions and handling
    // For now, just unlock with key from page 803
    // RTC->WPR |= (0xCA << RTC_WPR_KEY_Pos) & RTC_WPR_KEY_Msk;
    // RTC->WPR |= (0x53 << RTC_WPR_KEY_Pos) & RTC_WPR_KEY_Msk;
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    // Set INIT bit to 1 in the RTC_ISR register to enter initialization mode
    // In this mode, the calendar counter is stopped and its value can be updated
    RTC->ISR |= RTC_ISR_INIT;

    // Poll INITF bit of in the RTC_ISR register. The initialization phase mode is entered when
    //  INITF is set to 1. It takes from 1 to 2 RTCCLK clock cycles (due to clock synchronization)

    // TODO: make propper timeout
    while (false == (RTC->ISR & RTC_ISR_INITF))
        ;

    // To generate a 1 Hz clock for the calendar counter, program first the synchronous prescaler
    //  factor in RTC_PRER register, and then program the asynchronous prescaler factor
    // Even if only one of the two fields needs to be changed, 2 separate write accesses
    //  must be performed to the RTC_PRER register
    RTC->PRER |= (RTC_SYNC_PRESCAL << RTC_PRER_PREDIV_S_Pos) & RTC_PRER_PREDIV_S_Msk;
    RTC->PRER |= (RTC_ASYNC_PRESCAL << RTC_PRER_PREDIV_A_Pos) & RTC_PRER_PREDIV_A_Msk;

    // Load the initial time and date values in the shadow registers (RTC_TR and RTC_DR), and
    //  configure the time format (12 or 24 hours) through the FMT bit in the RTC_CR register

    RTC->CR |= (RTC_FORMAT_24_HOUR << RTC_CR_FMT_Pos) & RTC_CR_FMT_Msk;

    // The TR, DR registers are write protected. The write access procedure is
    //  described in RTC register write protection on RM 0090 page 803
    uint8_t yearTens   = (initial_time->date.year_bcd >> 4) & 0x0F;
    uint8_t yearUnits  = initial_time->date.year_bcd & 0x0F;
    uint8_t monthTens  = (initial_time->date.month_bcd >> 4) & 0x0F;
    uint8_t monthUnits = initial_time->date.month_bcd & 0x0F;
    uint8_t dayTens    = (initial_time->date.day_bcd >> 4) & 0x0F;
    uint8_t dayUnits   = initial_time->date.day_bcd & 0x0F;

    uint32_t DR = 0;

    DR |= ((uint32_t)yearUnits << RTC_DR_YU_Pos) & RTC_DR_YU_Msk;
    DR |= ((uint32_t)yearTens << RTC_DR_YT_Pos) & RTC_DR_YT_Msk;
    DR |= ((uint32_t)initial_time->date.weekday << RTC_DR_WDU_Pos) & RTC_DR_WDU_Msk;
    DR |= ((uint32_t)monthUnits << RTC_DR_MU_Pos) & RTC_DR_MU_Msk;
    DR |= ((uint32_t)monthTens << RTC_DR_MT_Pos) & RTC_DR_MT_Msk;
    DR |= ((uint32_t)dayUnits << RTC_DR_DU_Pos) & RTC_DR_DU_Msk;
    DR |= ((uint32_t)dayTens << RTC_DR_DT_Pos) & RTC_DR_DT_Msk;

    RTC->DR = DR;

    uint8_t hoursTens    = (initial_time->time.hours_bcd >> 4) & 0x0F;
    uint8_t hoursUnits   = initial_time->time.hours_bcd & 0x0F;
    uint8_t minutesTens  = (initial_time->time.minutes_bcd >> 4) & 0x0F;
    uint8_t minutesUnits = initial_time->time.minutes_bcd & 0x0F;
    uint8_t secondsTens  = (initial_time->time.seconds_bcd >> 4) & 0x0F;
    uint8_t secondsUnits = initial_time->time.seconds_bcd & 0x0F;

    uint32_t TR = 0;

    TR |= (RTC_FORMAT_24_HOUR << RTC_TR_PM_Pos) & RTC_TR_PM_Msk;
    TR |= ((uint32_t)hoursUnits << RTC_TR_HU_Pos) & RTC_TR_HU_Msk;
    TR |= ((uint32_t)hoursTens << RTC_TR_HT_Pos) & RTC_TR_HT_Msk;
    TR |= ((uint32_t)minutesUnits << RTC_TR_MNU_Pos) & RTC_TR_MNU_Msk;
    TR |= ((uint32_t)minutesTens << RTC_TR_MNT_Pos) & RTC_TR_MNT_Msk;
    TR |= ((uint32_t)secondsUnits << RTC_TR_SU_Pos) & RTC_TR_SU_Msk;
    TR |= ((uint32_t)secondsTens << RTC_TR_ST_Pos) & RTC_TR_ST_Msk;

    RTC->TR = TR;

    // 5. Exit the initialization mode by clearing the INIT bit. The actual calendar counter value is then automatically loaded and the counting restarts after 4 RTCCLK clock cycles.
    // When the initialization sequence is complete, the calendar starts counting.
    RTC->ISR &= ~(RTC_ISR_INIT);
    while (RTC->ISR & RTC_ISR_INITF)
        ;

    // Relock registers
#if defined(STM32F407xx)
    PWR->CR &= ~(PWR_CR_DBP);
#else
#error "Please define a MCU arch"
#endif

    return true;
}

bool PHAL_getTimeRTC(RTC_timestamp_t* currentTimestamp) {
    if (RTC->ISR & RTC_ISR_INITS) {
        uint8_t seconds = (RTC->TR & RTC_TR_SU_Msk) >> RTC_TR_SU_Pos;
        seconds += (uint8_t)(((RTC->TR & RTC_TR_ST_Msk) >> RTC_TR_ST_Pos) * 10);

        uint8_t minutes = (RTC->TR & RTC_TR_MNU_Msk) >> RTC_TR_MNU_Pos;
        minutes += (uint8_t)(((RTC->TR & RTC_TR_MNT_Msk) >> RTC_TR_MNT_Pos) * 10);

        uint8_t hours = (RTC->TR & RTC_TR_HU_Msk) >> RTC_TR_HU_Pos;
        hours += (uint8_t)(((RTC->TR & RTC_TR_HT_Msk) >> RTC_TR_HT_Pos) * 10);

        uint8_t day = (RTC->DR & RTC_DR_DU_Msk) >> RTC_DR_DU_Pos;
        day += (uint8_t)(((RTC->DR & RTC_DR_DT_Msk) >> RTC_DR_DT_Pos) * 10);

        uint8_t month = (RTC->DR & RTC_DR_MU_Msk) >> RTC_DR_MU_Pos;
        month += (uint8_t)(((RTC->DR & RTC_DR_MT_Msk) >> RTC_DR_MT_Pos) * 10);

        uint8_t year = (RTC->DR & RTC_DR_YU_Msk) >> RTC_DR_YU_Pos;
        year += (uint8_t)(((RTC->DR & RTC_DR_YT_Msk) >> RTC_DR_YT_Pos) * 10);

        currentTimestamp->date.year_bcd    = year;
        currentTimestamp->date.month_bcd   = month;
        currentTimestamp->date.day_bcd     = day;
        currentTimestamp->time.hours_bcd   = hours;
        currentTimestamp->time.minutes_bcd = minutes;
        currentTimestamp->time.seconds_bcd = seconds;
        currentTimestamp->time.time_format = RTC_FORMAT_24_HOUR;

        return true;
    }
    return false;
}