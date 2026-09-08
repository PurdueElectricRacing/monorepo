#include "switches.h"

#include <string.h>

#include "led.h"
#include "main.h"
#include "state.h"

#include "common/phal_F4/gpio/gpio.h"

static constexpr uint16_t ADC_MAX_COUNTS = 4095;

static constexpr int LV_24V_R1 = 47000;
static constexpr int LV_24V_R2 = 3400;
static constexpr int LV_5V_R1 = 4300;
static constexpr int LV_5V_R2 = 3400;
static constexpr int LV_3V3_R1 = 4300;
static constexpr int LV_3V3_R2 = 10000;

static constexpr int HP_CS_R1 = 180;
static constexpr int HP_CS_R2 = 330;
static constexpr int HP_CS_R3 = 500;

static constexpr float HP_CS_R_SENSE = 0.002f;
static constexpr uint16_t CS_GAIN = 100;

typedef struct {
    switches_t switch_id;
    GPIO_TypeDef *ctrl_port;
    uint8_t ctrl_pin;
    bool has_ctrl_output;
    int8_t led_id;
} pdu_switch_output_t;

static constexpr int8_t LED_NONE = -1;

static const pdu_switch_output_t PDU_SWITCH_OUTPUTS[] = {
    {.switch_id = SW_PUMP_1, .ctrl_port = PUMP_1_CTRL_GPIO_Port, .ctrl_pin = PUMP_1_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_PUMP_1},
    {.switch_id = SW_PUMP_2, .ctrl_port = PUMP_2_CTRL_GPIO_Port, .ctrl_pin = PUMP_2_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_PUMP_2},
    {.switch_id = SW_SDC, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_SDC},
    {.switch_id = SW_HXFAN, .ctrl_port = HXFAN_CTRL_GPIO_Port, .ctrl_pin = HXFAN_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_HXFAN},
    {.switch_id = SW_FAN_1, .ctrl_port = FAN_1_CTRL_GPIO_Port, .ctrl_pin = FAN_1_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_NONE},
    {.switch_id = SW_FAN_2, .ctrl_port = FAN_2_CTRL_GPIO_Port, .ctrl_pin = FAN_2_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_NONE},
    {.switch_id = SW_FAN_3, .ctrl_port = FAN_3_CTRL_GPIO_Port, .ctrl_pin = FAN_3_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_NONE},
    {.switch_id = SW_FAN_4, .ctrl_port = FAN_4_CTRL_GPIO_Port, .ctrl_pin = FAN_4_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_NONE},
    {.switch_id = SW_AMK1, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_NONE},
    {.switch_id = SW_AMK2, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_NONE},
    {.switch_id = SW_DASH, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_DASH},
    {.switch_id = SW_ABOX, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_ABOX},
    {.switch_id = SW_MAIN, .ctrl_port = MAIN_CTRL_GPIO_Port, .ctrl_pin = MAIN_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_MAIN},
    {.switch_id = SW_DLFR, .ctrl_port = DLFR_CTRL_GPIO_Port, .ctrl_pin = DLFR_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_DLFR},
    {.switch_id = SW_DLBK, .ctrl_port = DLBK_CTRL_GPIO_Port, .ctrl_pin = DLBK_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_DLBK},
    {.switch_id = SW_BLT, .ctrl_port = BLT_CTRL_GPIO_Port, .ctrl_pin = BLT_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_BLT},
    {.switch_id = SW_CRIT_5V, .ctrl_port = CRIT_5V_CTRL_GPIO_Port, .ctrl_pin = CRIT_5V_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_5V_CRIT},
    {.switch_id = SW_TV, .ctrl_port = TV_CTRL_GPIO_Port, .ctrl_pin = TV_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_TV},
    {.switch_id = SW_DAQ, .ctrl_port = nullptr, .ctrl_pin = 0, .has_ctrl_output = false, .led_id = LED_DAQ},
    {.switch_id = SW_FAN_5V, .ctrl_port = FAN_5V_CTRL_GPIO_Port, .ctrl_pin = FAN_5V_CTRL_Pin, .has_ctrl_output = true, .led_id = LED_5V_FAN},
};

static const pdu_switch_output_t *switches_get_output(switches_t switch_id) {
    for (uint32_t i = 0; i < (sizeof(PDU_SWITCH_OUTPUTS) / sizeof(PDU_SWITCH_OUTPUTS[0])); i++) {
        if (PDU_SWITCH_OUTPUTS[i].switch_id == switch_id) {
            return &PDU_SWITCH_OUTPUTS[i];
        }
    }

    return nullptr;
}

static uint16_t switches_convert_high_power_current_ma(uint16_t adc_counts) {
    adc_counts = (uint16_t)(adc_counts * ADC_REF_mV / ADC_MAX_COUNTS);
    adc_counts = (uint16_t)(adc_counts * (HP_CS_R1 + HP_CS_R2) / HP_CS_R2);
    adc_counts = (uint16_t)(adc_counts * HP_CS_R3 / (HP_CS_R1 + HP_CS_R2));
    return adc_counts;
}

static uint16_t switches_convert_low_power_current_ma(uint16_t adc_counts) {
    adc_counts = (uint16_t)(adc_counts * ADC_REF_mV / ADC_MAX_COUNTS);
    return adc_counts;
}

static uint16_t switches_convert_voltage_mv(uint16_t adc_counts, int r1_ohm, int r2_ohm) {
    adc_counts = (uint16_t)(adc_counts * (r1_ohm + r2_ohm) / r2_ohm);
    adc_counts = (uint16_t)(adc_counts * ADC_REF_mV / ADC_MAX_COUNTS);
    return adc_counts;
}

static void switches_select_mux_channel(uint8_t channel) {
    PHAL_GPIO_write(MUX_CTRL_A_GPIO_Port, MUX_CTRL_A_Pin, (channel & 0x01U) != 0U);
    PHAL_GPIO_write(MUX_CTRL_B_GPIO_Port, MUX_CTRL_B_Pin, (channel & 0x02U) != 0U);
    PHAL_GPIO_write(MUX_CTRL_C_GPIO_Port, MUX_CTRL_C_Pin, (channel & 0x04U) != 0U);
}

static void switches_update_mux_measurements(void) {
    const uint8_t sampled_channel = g_pdu_state.next_mux_channel;

    g_pdu_state.mux_adc_counts[sampled_channel] = adc_readings.mux_out;
    g_pdu_state.next_mux_channel                = (uint8_t)((sampled_channel + 1U) % 6U);

    switches_select_mux_channel(g_pdu_state.next_mux_channel);
}

static void switches_update_currents(void) {
    g_pdu_state.switch_current_ma[SW_PUMP_1] = switches_convert_high_power_current_ma(adc_readings.pump_1_imon);
    g_pdu_state.switch_current_ma[SW_PUMP_2] = switches_convert_high_power_current_ma(adc_readings.pump_2_imon);
    g_pdu_state.switch_current_ma[SW_SDC]    = switches_convert_high_power_current_ma(adc_readings.sdc_imon);
    g_pdu_state.switch_current_ma[SW_HXFAN]  = switches_convert_high_power_current_ma(adc_readings.hxfan_imon);

    g_pdu_state.switch_current_ma[SW_FAN_1] = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[0]);
    g_pdu_state.switch_current_ma[SW_FAN_2] = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[1]);
    g_pdu_state.switch_current_ma[SW_FAN_3] = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[2]);
    g_pdu_state.switch_current_ma[SW_FAN_4] = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[3]);
    g_pdu_state.switch_current_ma[SW_AMK1]  = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[4]);
    g_pdu_state.switch_current_ma[SW_AMK2]  = switches_convert_high_power_current_ma(g_pdu_state.mux_adc_counts[5]);

    g_pdu_state.switch_current_ma[SW_DASH] = switches_convert_low_power_current_ma(adc_readings.dash_cs);
    g_pdu_state.switch_current_ma[SW_ABOX] = switches_convert_low_power_current_ma(adc_readings.abox_cs);
    g_pdu_state.switch_current_ma[SW_MAIN] = switches_convert_low_power_current_ma(adc_readings.main_cs);
    g_pdu_state.switch_current_ma[SW_DLFR] = switches_convert_low_power_current_ma(adc_readings.dlfr_cs);
    g_pdu_state.switch_current_ma[SW_DLBK] = switches_convert_low_power_current_ma(adc_readings.dlbk_cs);

    uint16_t current_mv = adc_readings.v24_cs;
    current_mv = (uint16_t)(current_mv * ADC_REF_mV / ADC_MAX_COUNTS);
    current_mv = (uint16_t)(current_mv / HP_CS_R_SENSE / CS_GAIN);
    g_pdu_state.switch_current_ma[CS_24V] = current_mv;

    current_mv = adc_readings.v5_cs;
    current_mv = (uint16_t)(current_mv * ADC_REF_mV / ADC_MAX_COUNTS);
    g_pdu_state.switch_current_ma[CS_5V] = current_mv;
}

static void switches_update_voltages(void) {
    g_pdu_state.rail_voltage_mv.in_24v_mv  = switches_convert_voltage_mv(adc_readings.v24_vs, LV_24V_R1, LV_24V_R2);
    g_pdu_state.rail_voltage_mv.out_5v_mv  = switches_convert_voltage_mv(adc_readings.v5_vs, LV_5V_R1, LV_5V_R2);
    g_pdu_state.rail_voltage_mv.out_3v3_mv = switches_convert_voltage_mv(adc_readings.v3v3_vs, LV_3V3_R1, LV_3V3_R2);
}

void switches_init(void) {
    g_pdu_state.next_mux_channel = 0;
    memset(g_pdu_state.mux_adc_counts, 0, sizeof(g_pdu_state.mux_adc_counts));
    switches_select_mux_channel(g_pdu_state.next_mux_channel);
}

void switches_periodic(void) {
    switches_update_mux_measurements();
    switches_update_currents();
    switches_update_voltages();
}

void switches_set_state(switches_t switch_id, bool enabled) {
    if (switch_id == CS_24V || switch_id == CS_5V || switch_id == CS_SWITCH_COUNT) {
        return;
    }

    const pdu_switch_output_t *output = switches_get_output(switch_id);
    if (output == nullptr) {
        return;
    }

    if (output->has_ctrl_output) {
        PHAL_GPIO_write(output->ctrl_port, output->ctrl_pin, enabled);
    }

    if (output->led_id != LED_NONE) {
        LED_control(output->led_id, enabled ? LED_ON : LED_OFF);
    }
}

bool switches_is_enabled(switches_t switch_id) {
    const pdu_switch_output_t *output = switches_get_output(switch_id);
    if (output == nullptr || !output->has_ctrl_output) {
        return true;
    }

    return PHAL_GPIO_read(output->ctrl_port, output->ctrl_pin);
}

uint16_t switches_get_mux_adc_counts(uint8_t channel) {
    if (channel >= 6U) {
        return 0;
    }

    return g_pdu_state.mux_adc_counts[channel];
}

void switches_enable_default_rails(void) {
    switches_set_state(SW_SDC, true);
    switches_set_state(SW_DAQ, true);
    switches_set_state(SW_TV, true);
    switches_set_state(SW_MAIN, false);
    switches_set_state(SW_ABOX, true);
    switches_set_state(SW_DASH, true);
    switches_set_state(SW_CRIT_5V, true);
    switches_set_state(SW_BLT, true);
    switches_set_state(SW_HXFAN, true);
    switches_set_state(SW_DLBK, true);
    switches_set_state(SW_PUMP_1, true);
    switches_set_state(SW_PUMP_2, true);
}
