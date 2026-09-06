/**
 * @file driver_interface.h
 * @brief thread to manage driver-facing LCD, buttons, LEDs
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef DRIVER_INTERFACE_H
#define DRIVER_INTERFACE_H

#include <stdint.h>

typedef enum {
    UPDATE_PAGE,
    MENU_UP,
    MENU_DOWN,
    BACK_PAGE,
    FORWARD_PAGE,
    SELECT_BUTTON,
    START_BUTTON,
    MARK_DATA,
    TOGGLE_REGEN,
    RIGHT_WHEEL_MINUS,
    RIGHT_WHEEL_PLUS,
    LEFT_WHEEL_MINUS,
    LAP_SET
} driver_interface_action_t;

typedef enum {
    DI_STATE_LCD_INIT = 0,
    DI_STATE_BUTTONS_INIT = 1,
    DI_STATE_ACTIVE   = 2
} driver_interface_state_t;

static constexpr uint32_t DRIVER_INTERFACE_PERIOD_MS = 50;
void driver_interface_periodic(void);

// Button EXTI handlers
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI4_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void EXTI15_10_IRQHandler(void);

#endif // DRIVER_INTERFACE_H