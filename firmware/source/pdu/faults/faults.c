#include "faults.h"

#include "can_library/faults_common.h"
#include "led.h"
#include "main.h"
#include "state.h"

#include "common/phal_F4/gpio/gpio.h"

typedef struct {
    fault_id_t fault_id;
    GPIO_TypeDef *nflt_port;
    uint8_t nflt_pin;
    int led_id;
} pdu_rail_fault_map_t;

static const pdu_rail_fault_map_t PDU_RAIL_FAULT_MAP[] = {
    {.fault_id = FAULT_ID_DASH_RAIL, .nflt_port = DASH_NFLT_GPIO_Port, .nflt_pin = DASH_NFLT_Pin, .led_id = LED_DASH},
    {.fault_id = FAULT_ID_ABOX_RAIL, .nflt_port = ABOX_NFLT_GPIO_Port, .nflt_pin = ABOX_NFLT_Pin, .led_id = LED_ABOX},
    {.fault_id = FAULT_ID_MAIN_RAIL, .nflt_port = MAIN_NFLT_GPIO_Port, .nflt_pin = MAIN_NFLT_Pin, .led_id = LED_MAIN},
    {.fault_id = FAULT_ID_V_CRIT, .nflt_port = CRIT_5V_NFLT_GPIO_Port, .nflt_pin = CRIT_5V_NFLT_Pin, .led_id = LED_5V_CRIT},
    {.fault_id = FAULT_ID_V_NONCRIT, .nflt_port = TV_NFLT_GPIO_Port, .nflt_pin = TV_NFLT_Pin, .led_id = LED_TV},
    {.fault_id = FAULT_ID_FRONT_DRIVELINE_RAIL, .nflt_port = DLFR_NFLT_GPIO_Port, .nflt_pin = DLFR_NFLT_Pin, .led_id = LED_DLFR},
    {.fault_id = FAULT_ID_REAR_DRIVELINE_RAIL, .nflt_port = DLBK_NFLT_GPIO_Port, .nflt_pin = DLBK_NFLT_Pin, .led_id = LED_DLBK},
    {.fault_id = FAULT_ID_BULLET_RAIL, .nflt_port = BLT_NFLT_GPIO_Port, .nflt_pin = BLT_NFLT_Pin, .led_id = LED_BLT},
};

void faults_init(void) {
    g_pdu_state.next_rail_fault_index = 0;
}

void faults_periodic(void) {
    const uint8_t index = g_pdu_state.next_rail_fault_index;
    const pdu_rail_fault_map_t *fault = &PDU_RAIL_FAULT_MAP[index];

    const bool is_faulted = !PHAL_GPIO_read(fault->nflt_port, fault->nflt_pin);
    update_fault(fault->fault_id, is_faulted);
    LED_control(fault->led_id, is_latched(fault->fault_id) ? LED_BLINK : LED_OFF);

    g_pdu_state.next_rail_fault_index = index + 1U;
    if (g_pdu_state.next_rail_fault_index >= (sizeof(PDU_RAIL_FAULT_MAP) / sizeof(PDU_RAIL_FAULT_MAP[0]))) {
        g_pdu_state.next_rail_fault_index = 0;
    }
}
