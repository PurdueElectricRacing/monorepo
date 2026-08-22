#include "telemetry.h"

#include "can_library/faults_common.h"
#include "can_library/generated/PDU.h"
#include "can_library/generated/VCAN.h"
#include "main.h"
#include "source/a_box/telemetry/telemetry.h"
#include "state.h"
#include "switches.h"

static uint16_t telemetry_internal_temp_c(uint16_t internal_therm_adc_counts) {
    float vsense = (internal_therm_adc_counts / (float)4095) * 3.3f;
    float temp_c = ((vsense - 0.76f) / 0.0025f) + 25.0f;
    if (temp_c <= 0.0f) {
        return 0;
    }

    if (temp_c >= 65535.0f) {
        return 65535;
    }

    return (uint16_t)temp_c;
}

/**
 * @brief Reports telemetry data at 10 Hz rate
 * Includes: Rail Voltages and Currents, Flow Rates
 */
static_assert(V_RAILS_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(OTHER_CURRENTS_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(PUMP_AND_FAN_CURRENT_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(V_RAILS_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(FAN_CURRENT2_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(RAIL_CURRENTS_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);
static_assert(PDU_TEMPS_PERIOD_MS == TELEMETRY_10HZ_PERIOD_MS);

void telemetry_10hz(void) {
    update_fault(FAULT_ID_LV_GETTING_LOW, g_pdu_state.rail_voltage_mv.in_24v_mv);
    update_fault(FAULT_ID_LV_CRITICAL_LOW, g_pdu_state.rail_voltage_mv.in_24v_mv);

    CAN_SEND_v_rails(
        g_pdu_state.rail_voltage_mv.in_24v_mv,
        g_pdu_state.rail_voltage_mv.out_5v_mv,
        g_pdu_state.rail_voltage_mv.out_3v3_mv,
        0
    );

    CAN_SEND_rail_currents(g_pdu_state.switch_current_ma[CS_24V], g_pdu_state.switch_current_ma[CS_5V]);

    CAN_SEND_pump_and_fan_current(
        g_pdu_state.switch_current_ma[SW_PUMP_1],
        g_pdu_state.switch_current_ma[SW_PUMP_2],
        g_pdu_state.switch_current_ma[SW_FAN_1],
        g_pdu_state.switch_current_ma[SW_FAN_2]
    );

    CAN_SEND_fan_current2(g_pdu_state.switch_current_ma[SW_FAN_3], g_pdu_state.switch_current_ma[SW_FAN_4]);

    CAN_SEND_other_currents(
        g_pdu_state.switch_current_ma[SW_SDC],
        g_pdu_state.switch_current_ma[SW_HXFAN],
        g_pdu_state.switch_current_ma[SW_DASH],
        g_pdu_state.switch_current_ma[SW_ABOX],
        g_pdu_state.switch_current_ma[SW_MAIN]
    );

    CAN_SEND_pdu_temps(telemetry_internal_temp_c(adc_readings.internal_therm));
}
