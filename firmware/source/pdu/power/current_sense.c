/**
 * @file current_sense.c
 * @brief current sensing
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "current_sense.h"
#include "main.h"
#include "switch.h"
#include "common/rtos/rtos.h"
#include "can_library/generated/PDU.h"
#include "can_library/faults_common.h"



static void mux_select(mux_id_t mux_id, uint8_t mux_addr) {
    switch(mux_id) {
        case ANALOG_MUX1:
            PHAL_GPIO_write(MUX1_CTRL_A_PORT, MUX1_CTRL_A_PIN, (mux_addr >> 0) & 0x1);
            PHAL_GPIO_write(MUX1_CTRL_B_PORT, MUX1_CTRL_B_PIN, (mux_addr >> 1) & 0x1);
            PHAL_GPIO_write(MUX1_CTRL_C_PORT, MUX1_CTRL_C_PIN, (mux_addr >> 2) & 0x1);
            break;
        case ANALOG_MUX2:
            PHAL_GPIO_write(MUX2_CTRL_A_PORT, MUX2_CTRL_A_PIN, (mux_addr >> 0) & 0x1);
            PHAL_GPIO_write(MUX2_CTRL_B_PORT, MUX2_CTRL_B_PIN, (mux_addr >> 1) & 0x1);
            PHAL_GPIO_write(MUX2_CTRL_C_PORT, MUX2_CTRL_C_PIN, (mux_addr >> 2) & 0x1);
            PHAL_GPIO_write(MUX2_CTRL_D_PORT, MUX2_CTRL_D_PIN, (mux_addr >> 3) & 0x1);
            break;
    }
}

static float adc_to_current_ma(uint16_t adc_value) {
    float v_adc = (float)adc_value * (ADC_VREF_V / ADC_MAX_COUNTS);
    float v_imon = v_adc / SWITCH_SENSE_DIVIDER_GAIN;
    float i_imon = v_imon / (SWITCH_IMON_R1_OHMS + SWITCH_IMON_R2_OHMS);
    return i_imon * SWITCH_CURRENT_SENSE_GAIN * 1000.0f;
}

static switch_id_t g_switch_poll_index = 0;

static_assert(SWITCH_CS_PERIOD_MS == CURRENT_SENSE_TASK_PERIOD_MS);
void current_sense_periodic() {
    switch_t* sw = &g_switches[g_switch_poll_index];

    mux_select(sw->mux_id, sw->mux_addr);
    RTOS_delay_ms(MUX_SETTLE_DELAY_MS); // wait for mux to settle

    uint16_t adc_value = (sw->mux_id == ANALOG_MUX1) ? raw_adc2_values.mux1_out : raw_adc2_values.mux2_out;
    float current_ma = adc_to_current_ma(adc_value);

    bool is_switch_faulted = current_ma >= SWITCH_FAULT_CURRENT_THRESHOLD_MA;
    update_fault(sw->fault_id, is_switch_faulted);
    if (!is_switch_faulted) {
        // todo set current to an invalid value
        sw->current_ma = current_ma;
    }

    CAN_SEND_switch_cs(g_switch_poll_index, (uint16_t)sw->current_ma);

    g_switch_poll_index = (g_switch_poll_index + 1) % NUM_SWITCHES;
}