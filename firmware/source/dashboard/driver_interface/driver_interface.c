/**
 * @file driver_interface.c
 * @brief thread to manage driver-facing LCD, buttons, LEDs
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "driver_interface.h"

#include "can_library/faults_common.h"
#include "can_library/generated/DASHBOARD.h"
#include "can_library/generated/can_types.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/exti/exti.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/watchdog/watchdog.h"
#include "common/utils/countof.h"
#include "lap_timer.h"
#include "lcd.h"
#include "main.h"
#include "pages/vcu.h"

extern void HardFault_Handler(void);

static driver_interface_state_t di_state = DI_STATE_LCD_INIT;
static driver_interface_state_t next_di_state = DI_STATE_LCD_INIT;

#define ACTION_QUEUE_LENGTH (10)
RTOS_DEFINE_QUEUE(action_queue, driver_interface_action_t, ACTION_QUEUE_LENGTH);
volatile uint16_t data_mark_index = 0;

static constexpr uint32_t INTERRUPT_DEBOUNCE_MS = 150;

static const PHAL_EXTI_InitConfig_t button_exti_config[] = {
    {.bank = EBB_MINUS_PORT, .pin = EBB_MINUS_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = EBB_PLUS_PORT, .pin = EBB_PLUS_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = REGEN_TOGGLE_PORT, .pin = REGEN_TOGGLE_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = MARK_DATA_PORT, .pin = MARK_DATA_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = UP_BUTTON_PORT, .pin = UP_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = DOWN_BUTTON_PORT, .pin = DOWN_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = RIGHT_BUTTON_PORT, .pin = RIGHT_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = LEFT_BUTTON_PORT, .pin = LEFT_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = LAP_SET_PORT, .pin = LAP_SET_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = TV1_MINUS_PORT, .pin = TV1_MINUS_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = START_BUTTON_PORT, .pin = START_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
    {.bank = SELECT_BUTTON_PORT, .pin = SELECT_BUTTON_PIN, .trigger = PHAL_EXTI_TRIGGER_FALLING},
};

static void enqueue_button_action(uint8_t pin, driver_interface_action_t action) {
    static uint32_t last_interrupt_time[16] = {0};
    uint32_t now                            = xTaskGetTickCountFromISR();
    if (now - last_interrupt_time[pin] <= INTERRUPT_DEBOUNCE_MS) {
        return;
    }

    last_interrupt_time[pin] = now;
    xQueueSendFromISR(action_queue, &action, NULL);
}

void PHAL_EXTI_callback(GPIO_TypeDef *bank, uint8_t pin) {
    if (bank == EBB_MINUS_PORT && pin == EBB_MINUS_PIN) {
        enqueue_button_action(pin, RIGHT_WHEEL_MINUS);
    } else if (bank == EBB_PLUS_PORT && pin == EBB_PLUS_PIN) {
        enqueue_button_action(pin, RIGHT_WHEEL_PLUS);
    } else if (bank == REGEN_TOGGLE_PORT && pin == REGEN_TOGGLE_PIN) {
        enqueue_button_action(pin, TOGGLE_REGEN);
    } else if (bank == MARK_DATA_PORT && pin == MARK_DATA_PIN) {
        enqueue_button_action(pin, MARK_DATA);
    } else if (bank == UP_BUTTON_PORT && pin == UP_BUTTON_PIN) {
        enqueue_button_action(pin, MENU_UP);
    } else if (bank == DOWN_BUTTON_PORT && pin == DOWN_BUTTON_PIN) {
        enqueue_button_action(pin, MENU_DOWN);
    } else if (bank == RIGHT_BUTTON_PORT && pin == RIGHT_BUTTON_PIN) {
        enqueue_button_action(pin, FORWARD_PAGE);
    } else if (bank == LEFT_BUTTON_PORT && pin == LEFT_BUTTON_PIN) {
        enqueue_button_action(pin, BACK_PAGE);
    } else if (bank == LAP_SET_PORT && pin == LAP_SET_PIN) {
        enqueue_button_action(pin, LAP_SET);
    } else if (bank == TV1_MINUS_PORT && pin == TV1_MINUS_PIN) {
        enqueue_button_action(pin, LEFT_WHEEL_MINUS);
    } else if (bank == START_BUTTON_PORT && pin == START_BUTTON_PIN) {
        enqueue_button_action(pin, START_BUTTON);
    } else if (bank == SELECT_BUTTON_PORT && pin == SELECT_BUTTON_PIN) {
        enqueue_button_action(pin, SELECT_BUTTON);
    }
}

static void init_buttons(void) {
    RTOS_INIT_QUEUE(action_queue);

    NVIC_SetPriority(EXTI0_IRQn, 7U);
    NVIC_SetPriority(EXTI1_IRQn, 7U);
    NVIC_SetPriority(EXTI4_IRQn, 7U);
    NVIC_SetPriority(EXTI9_5_IRQn, 7U);
    NVIC_SetPriority(EXTI15_10_IRQn, 7U);

    if (!PHAL_EXTI_init(button_exti_config, countof(button_exti_config))) {
        HardFault_Handler();
    }
}

void action_dispatcher(void) {
    // non blocking rx
    driver_interface_action_t action;
    while (xQueueReceive(action_queue, &action, 0) == pdTRUE) {
        switch (action) {
            case UPDATE_PAGE:
                updatePage();
                break;
            case MENU_UP:
                moveUp();
                break;
            case MENU_DOWN:
                moveDown();
                break;
            case BACK_PAGE:
                backPage();
                break;
            case FORWARD_PAGE:
                advancePage();
                break;
            case SELECT_BUTTON:
                selectItem();
                break;
            case START_BUTTON:
                CAN_SEND_start_button(true);
                break;
            case MARK_DATA: {
                CAN_SEND_mark_data(xTaskGetTickCount(), data_mark_index);
                data_mark_index++;
                break;
            }
            case TOGGLE_REGEN: {
                vcu_toggle_regen();
                break;
            }
            case RIGHT_WHEEL_MINUS: {
                vcu_wheel_adjust(true, -1);
                break;
            }
            case RIGHT_WHEEL_PLUS: {
                vcu_wheel_adjust(true, 1);
                break;
            }
            case LEFT_WHEEL_MINUS: {
                vcu_wheel_adjust(false, -1);
                break;
            }
            case LAP_SET: {
                lap_timer_onpress();
                break;
            }
        }
    }
}

extern status_leds_t status_leds;
void set_external_leds(void) {
    // dont update the external LEDS until we're out of preflight
    if (status_leds.state == HEARTBEAT_STATE_PREFLIGHT) {
        return;
    }

    bool precharge_complete = is_clear(FAULT_ID_PRECHARGE_INCOMPLETE);
    PHAL_GPIO_write(PRCHG_LED_PORT, PRCHG_LED_PIN, precharge_complete);

    bool imd_faulted = is_latched(FAULT_ID_SDC1_IMD);
    PHAL_GPIO_write(IMD_LED_PORT, IMD_LED_PIN, imd_faulted);

    bool bms_faulted = is_latched(FAULT_ID_BMS_DISCONNECTED);
    PHAL_GPIO_write(BMS_LED_PORT, BMS_LED_PIN, bms_faulted);
    
    if (can_data.vcu_settings.is_stale) {
        // default off
        PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, false);
        return;
    }

    bool is_regen_enabled = can_data.vcu_settings.is_regen_enabled;
    PHAL_GPIO_write(REGEN_LED_PORT, REGEN_LED_PIN, is_regen_enabled);
}

void driver_interface_periodic(void) {
    di_state = next_di_state;
    next_di_state = di_state;

    switch (di_state) {
        case DI_STATE_LCD_INIT: {
            if (!was_reset_by_WDG()) {
                RTOS_delay_ms(1000); // wait a bit for LCD to power-on
            }
            LCD_init(LCD_BAUD_RATE);
            next_di_state = DI_STATE_BUTTONS_INIT;
            break;
        }
        case DI_STATE_BUTTONS_INIT: {
            init_buttons();
            next_di_state = DI_STATE_ACTIVE;
        }
        case DI_STATE_ACTIVE: {
            set_external_leds();
            action_dispatcher();
            updateTelemetryPages();
            LCD_tx_update(); // dump the command
            break;
        }
    }
}