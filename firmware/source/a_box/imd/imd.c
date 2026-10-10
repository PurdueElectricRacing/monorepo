/**
 * @file imd.c
 * @brief ABOX IMD PWM measurement and state reporting
 *
 * @author Aditya Saini (saini19@purdue.edu)
 */

#include "imd.h"

#include "can_library/generated/A_BOX.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "main.h"

static uint8_t g_imd_duty_pct = 0;

static bool imd_freq_near(uint8_t freq_hz, uint8_t nominal_hz) {
    uint8_t low  = (nominal_hz > IMD_FREQ_TOLERANCE_HZ) ? (nominal_hz - IMD_FREQ_TOLERANCE_HZ) : 0;
    uint8_t high = nominal_hz + IMD_FREQ_TOLERANCE_HZ;
    return (freq_hz >= low) && (freq_hz <= high);
}

static imd_state_t imd_state_from_measurement(bool valid, uint8_t freq_hz) {
    if (!valid) {
        return IMD_STATE_OFF_OR_SHORT_CIRCUIT;
    }
    if (imd_freq_near(freq_hz, IMD_FREQ_NORMAL_HZ)) return IMD_STATE_NORMAL;
    if (imd_freq_near(freq_hz, IMD_FREQ_UNDERVOLTAGE_HZ)) return IMD_STATE_UNDERVOLTAGE;
    if (imd_freq_near(freq_hz, IMD_FREQ_SPEED_START_HZ)) return IMD_STATE_SPEED_START;
    if (imd_freq_near(freq_hz, IMD_FREQ_DEVICE_ERROR_HZ)) return IMD_STATE_DEVICE_ERROR;
    if (imd_freq_near(freq_hz, IMD_FREQ_CONN_FAULT_HZ)) return IMD_STATE_CONNECTION_FAULT;
    return IMD_STATE_UNKNOWN;
}

// set up TIM1 to measure PWM on PA8 for us
void imd_init(void) {
    TIM_TypeDef *tim = IMD_PWM_LS_TIM;

    // turn TIM1's clock on
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    tim->CR1 = 0U;

    // get clock speed
    uint32_t timer_clock_hz = PHAL_RCC_getAPB2ClockHz();
    // divide to get 100 kHz (1 tick = 10 us)
    uint32_t divider = (timer_clock_hz + (IMD_TIMER_TICK_HZ / 2U)) / IMD_TIMER_TICK_HZ;
    if (divider == 0U) {
        divider = 1U;
    }
    // Prescaler and Autoreload (timers max count)
    tim->PSC = (uint16_t)(divider - 1U);
    tim->ARR = IMD_TIMER_ARR;

    // look at the same pin. apply input filter to noise
    tim->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_0 
                | TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_0;

    // capture rising edges (period), falling edges (time high)
    tim->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P;
    // reset the counter on every rising edge 
    tim->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;

    tim->EGR = TIM_EGR_UG;
    tim->SR  = 0U;
    tim->CR1 = TIM_CR1_CEN;
}

// runs once a second: read the timer, figure out IMD state, send it on CAN
void imd_periodic(void) {
    TIM_TypeDef *tim = IMD_PWM_LS_TIM;

    uint32_t sr = tim->SR;
    bool captured = (sr & TIM_SR_CC1IF) != 0U; // rising edge capture
    uint32_t period_ticks = tim->CCR1;         
    uint32_t high_ticks = tim->CCR2;           
    bool overflow = (sr & TIM_SR_UIF) != 0U;   
    tim->SR &= ~TIM_SR_UIF;

    // need a capture, no overflow, and period != 0
    bool valid = captured && !overflow && (period_ticks != 0U);
    uint8_t freq_hz = 0;
    if (valid) {
        freq_hz = (uint8_t)((IMD_TIMER_TICK_HZ + (period_ticks / 2U)) / period_ticks);
        g_imd_duty_pct = (uint8_t)(((high_ticks * 100U) + (period_ticks / 2U)) / period_ticks);
    } else {
        g_imd_duty_pct = 0;
    }

    imd_state_t state = imd_state_from_measurement(valid, freq_hz);
    bool imd_status = PHAL_GPIO_read(IMD_STATUS_PORT, IMD_STATUS_PIN);

    CAN_SEND_imd_state(state, imd_status);
}

static_assert(IMD_STATE_PERIOD_MS == IMD_PERIOD_MS);
