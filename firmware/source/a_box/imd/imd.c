/**
 * @file imd.c
 * @brief ABOX IMD PWM capture and CAN reporting
 *
 * @author Aditya Saini (saini19@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "imd.h"
#include "imd_decode.h"

#include "can_library/generated/A_BOX.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/rtos/rtos.h"
#include "main.h"


// PC12 AF1 routes IMD LS to TIM5_CH2 (TI2).
static volatile imd_capture_tracker_t g_capture = {0};


void imd_init(void) {
    TIM_TypeDef *tim = TIM5;

    // Turn TIM5's clock on
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
    (void)RCC->APB1ENR1; // Allow the peripheral clock enable to propagate
    NVIC_DisableIRQ(TIM5_IRQn);
    tim->CR1 = 0U;
    tim->DIER = 0U;
    g_capture = (imd_capture_tracker_t){0};

    // Get clock speed
    uint32_t timer_clock_hz = PHAL_RCC_getAPB1ClockHz();
    // Divide to get 100 kHz (1 tick = 10 us)
    uint32_t divider = (timer_clock_hz + (IMD_TIMER_TICK_HZ / 2U)) / IMD_TIMER_TICK_HZ;
    if (divider == 0U) {
        divider = 1U;
    }
    // Prescaler and Autoreload (timers max count)
    tim->PSC = (uint16_t)(divider - 1U);
    tim->ARR = IMD_TIMER_ARR;

    // Channel 1 and channel 2 are both connected to the same input
    // Apply filtering to both (8 consecutive samples to accept input change)
    tim->CCMR1 = TIM_CCMR1_CC2S_0   // Channel 2 captures input TI2 directly
                 | TIM_CCMR1_IC2F_1 // IC2F = 0011: apply 8-sample filtering to channel 2
                 | TIM_CCMR1_IC2F_0
                 | TIM_CCMR1_CC1S_1 // Channel 1 also captures TI2, through its indirect input
                 | TIM_CCMR1_IC1F_1 // IC1F = 0011: apply 8-sample filtering to channel 1
                 | TIM_CCMR1_IC1F_0;

    // Capture the period in CCR2 and elapsed high time in CCR1
    tim->CCER = TIM_CCER_CC2E    // Enable channel 2; CC2P/CC2NP = 0 selects rising edges
                | TIM_CCER_CC1E  // Enable channel 1 capture on the same TI2 input
                | TIM_CCER_CC1P; // CC1P = 1, CC1NP = 0 selects falling edges

    // Use the filtered TI2 rising edge to reset the counter after capturing CCR2
    tim->SMCR = TIM_SMCR_TS_2     // TS = 110: select TI2FP2 as the slave-mode trigger
                | TIM_SMCR_TS_1
                | TIM_SMCR_SMS_2; // SMS = 0100: reset mode, starting a new period at each trigger

    // Load the prescaler and initialize the counter before enabling capture interrupts
    tim->EGR = TIM_EGR_UG; // Generate an update event to load buffered timer settings
    tim->SR  = 0U;         // Clear timer flags, including the update flag set by UG
    
    // Setup interrupts
    NVIC_ClearPendingIRQ(TIM5_IRQn);
    NVIC_SetPriority(TIM5_IRQn, IMD_CAPTURE_IRQ_PRIORITY);
    
    tim->DIER = TIM_DIER_CC2IE;   // Interrupt on each channel 2 (rising-edge) capture
    NVIC_EnableIRQ(TIM5_IRQn);
    tim->CR1 = TIM_CR1_URS     // Restrict update requests to overflow/underflow, excluding trigger resets
               | TIM_CR1_CEN;  // Start the timer counter
}

// CCR reads clear the corresponding CCxIF in input-capture mode
// Require a falling edge since the preceding rising-edge ISR
// Overcapture or a new edge during the read results in a lost capture
void TIM5_IRQHandler(void) {
    TIM_TypeDef *tim = TIM5;
    uint32_t before = tim->SR;
    if ((before & TIM_SR_CC2IF) == 0U) { // No rising-edge capture to process
        return;
    }

    uint32_t period_ticks = tim->CCR2; // Time between rising edges; clears CC2IF
    uint32_t high_ticks = tim->CCR1;   // Time from rising to falling edge; clears CC1IF
    uint32_t after = tim->SR;          // Check for edges or errors arriving during the reads
    uint32_t errors = TIM_SR_UIF | TIM_SR_CC1OF | TIM_SR_CC2OF; // Overflow or overwritten captures

    // Require a falling edge, no errors, and no new captures during acquisition
    bool cycle_ok = (before & TIM_SR_CC1IF) != 0U &&
        ((before | after) & errors) == 0U &&
        (after & (TIM_SR_CC1IF | TIM_SR_CC2IF)) == 0U;

    // SR flags: write zero only to observed errors
    // without accidentally clearing a newly arrived capture flag
    tim->SR = ~((before | after) & errors);

    uint32_t now_ms = xTaskGetTickCountFromISR();
    imd_capture_update(&g_capture, period_ticks, high_ticks, now_ms, cycle_ok);
}


void imd_periodic(void) {
    taskENTER_CRITICAL();
    imd_capture_t capture = g_capture.latest;
    uint32_t now_ms = xTaskGetTickCount();
    taskEXIT_CRITICAL();

    imd_measurement_t measurement = imd_decode(&capture, now_ms);
    bool imd_status = PHAL_GPIO_read(IMD_STATUS_PORT, IMD_STATUS_PIN);
    CAN_SEND_imd_state(
        measurement.state,
        imd_status,
        measurement.quality,
        measurement.r_iso_kohm
    );
}

static_assert(IMD_STATE_PERIOD_MS == IMD_PERIOD_MS);
