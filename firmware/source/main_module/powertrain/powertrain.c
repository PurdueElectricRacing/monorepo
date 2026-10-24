/**
 * @file powertrain.c
 * @brief Powertrain implementation
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "powertrain.h"

#include "can_library/faults_common.h"
#include "can_library/generated/MAIN_MODULE.h"

powertrain_t g_powertrain;

// The vehicle FSM calls the powertrain often enough to flush every inverter setpoint.
static_assert(POWERTRAIN_PERIOD_MS == INVA_SET_PERIOD_MS);
static_assert(POWERTRAIN_PERIOD_MS == INVB_SET_PERIOD_MS);
static_assert(POWERTRAIN_PERIOD_MS == INVC_SET_PERIOD_MS);
static_assert(POWERTRAIN_PERIOD_MS == INVD_SET_PERIOD_MS);

// ! important: assert that the layout hashes of all AMK messages match
static_assert(INVA_SET_LAYOUT_HASH == INVB_SET_LAYOUT_HASH, "AMK INVA/B Struct Mismatch");
static_assert(INVA_SET_LAYOUT_HASH == INVC_SET_LAYOUT_HASH, "AMK INVA/C Struct Mismatch");
static_assert(INVA_SET_LAYOUT_HASH == INVD_SET_LAYOUT_HASH, "AMK INVA/D Struct Mismatch");
static_assert(INVA_CRIT_LAYOUT_HASH == INVB_CRIT_LAYOUT_HASH, "AMK INVA/B Crit Struct Mismatch");
static_assert(INVA_CRIT_LAYOUT_HASH == INVC_CRIT_LAYOUT_HASH, "AMK INVA/C Crit Struct Mismatch");
static_assert(INVA_CRIT_LAYOUT_HASH == INVD_CRIT_LAYOUT_HASH, "AMK INVA/D Crit Struct Mismatch");
static_assert(INVA_INFO_LAYOUT_HASH == INVB_INFO_LAYOUT_HASH, "AMK INVA/B Info Struct Mismatch");
static_assert(INVA_INFO_LAYOUT_HASH == INVC_INFO_LAYOUT_HASH, "AMK INVA/C Info Struct Mismatch");
static_assert(INVA_INFO_LAYOUT_HASH == INVD_INFO_LAYOUT_HASH, "AMK INVA/D Info Struct Mismatch");
static_assert(INVA_ERR_1_LAYOUT_HASH == INVB_ERR_1_LAYOUT_HASH, "AMK INVA/B Err1 Struct Mismatch");
static_assert(INVA_ERR_1_LAYOUT_HASH == INVC_ERR_1_LAYOUT_HASH, "AMK INVA/C Err1 Struct Mismatch");
static_assert(INVA_ERR_1_LAYOUT_HASH == INVD_ERR_1_LAYOUT_HASH, "AMK INVA/D Err1 Struct Mismatch");
static_assert(INVA_ERR_2_LAYOUT_HASH == INVB_ERR_2_LAYOUT_HASH, "AMK INVA/B Err2 Struct Mismatch");
static_assert(INVA_ERR_2_LAYOUT_HASH == INVC_ERR_2_LAYOUT_HASH, "AMK INVA/C Err2 Struct Mismatch");
static_assert(INVA_ERR_2_LAYOUT_HASH == INVD_ERR_2_LAYOUT_HASH, "AMK INVA/D Err2 Struct Mismatch");
static_assert(INVA_TEMPS_LAYOUT_HASH == INVB_TEMPS_LAYOUT_HASH, "AMK INVA/B Temps Struct Mismatch");
static_assert(INVA_TEMPS_LAYOUT_HASH == INVC_TEMPS_LAYOUT_HASH, "AMK INVA/C Temps Struct Mismatch");
static_assert(INVA_TEMPS_LAYOUT_HASH == INVD_TEMPS_LAYOUT_HASH, "AMK INVA/D Temps Struct Mismatch");

static void flush_inva(void) {
    CAN_SEND_INVA_SET(
        g_powertrain.front_right.set.AMK_Control_bReserve,
        g_powertrain.front_right.set.AMK_Control_bInverterOn,
        g_powertrain.front_right.set.AMK_Control_bDcOn,
        g_powertrain.front_right.set.AMK_Control_bEnable,
        g_powertrain.front_right.set.AMK_Control_bErrorReset,
        g_powertrain.front_right.set.AMK_Control_bReserve2,
        g_powertrain.front_right.set.AMK_TorqueSetpoint,
        g_powertrain.front_right.set.AMK_PositiveTorqueLimit,
        g_powertrain.front_right.set.AMK_NegativeTorqueLimit
    );
}

static void flush_invb(void) {
    CAN_SEND_INVB_SET(
        g_powertrain.front_left.set.AMK_Control_bReserve,
        g_powertrain.front_left.set.AMK_Control_bInverterOn,
        g_powertrain.front_left.set.AMK_Control_bDcOn,
        g_powertrain.front_left.set.AMK_Control_bEnable,
        g_powertrain.front_left.set.AMK_Control_bErrorReset,
        g_powertrain.front_left.set.AMK_Control_bReserve2,
        g_powertrain.front_left.set.AMK_TorqueSetpoint,
        g_powertrain.front_left.set.AMK_PositiveTorqueLimit,
        g_powertrain.front_left.set.AMK_NegativeTorqueLimit
    );
}

static void flush_invc(void) {
    CAN_SEND_INVC_SET(
        g_powertrain.rear_left.set.AMK_Control_bReserve,
        g_powertrain.rear_left.set.AMK_Control_bInverterOn,
        g_powertrain.rear_left.set.AMK_Control_bDcOn,
        g_powertrain.rear_left.set.AMK_Control_bEnable,
        g_powertrain.rear_left.set.AMK_Control_bErrorReset,
        g_powertrain.rear_left.set.AMK_Control_bReserve2,
        g_powertrain.rear_left.set.AMK_TorqueSetpoint,
        g_powertrain.rear_left.set.AMK_PositiveTorqueLimit,
        g_powertrain.rear_left.set.AMK_NegativeTorqueLimit
    );
}

static void flush_invd(void) {
    CAN_SEND_INVD_SET(
        g_powertrain.rear_right.set.AMK_Control_bReserve,
        g_powertrain.rear_right.set.AMK_Control_bInverterOn,
        g_powertrain.rear_right.set.AMK_Control_bDcOn,
        g_powertrain.rear_right.set.AMK_Control_bEnable,
        g_powertrain.rear_right.set.AMK_Control_bErrorReset,
        g_powertrain.rear_right.set.AMK_Control_bReserve2,
        g_powertrain.rear_right.set.AMK_TorqueSetpoint,
        g_powertrain.rear_right.set.AMK_PositiveTorqueLimit,
        g_powertrain.rear_right.set.AMK_NegativeTorqueLimit
    );
}

void powertrain_init(void) {
    // Inverter A
    const AMK_config_t front_right_config = {
        .set_function = flush_inva,
        .crit         = (INVA_CRIT_data_t *)&can_data.INVA_CRIT,
        .info         = (INVA_INFO_data_t *)&can_data.INVA_INFO,
        .temps        = (INVA_TEMPS_data_t *)&can_data.INVA_TEMPS,
        .err1         = (INVA_ERR_1_data_t *)&can_data.INVA_ERR_1,
        .err2         = (INVA_ERR_2_data_t *)&can_data.INVA_ERR_2,
    };
    AMK_init(&g_powertrain.front_right, &front_right_config);

    // Inverter B
    const AMK_config_t front_left_config = {
        .set_function = flush_invb,
        .crit         = (INVA_CRIT_data_t *)&can_data.INVB_CRIT,
        .info         = (INVA_INFO_data_t *)&can_data.INVB_INFO,
        .temps        = (INVA_TEMPS_data_t *)&can_data.INVB_TEMPS,
        .err1         = (INVA_ERR_1_data_t *)&can_data.INVB_ERR_1,
        .err2         = (INVA_ERR_2_data_t *)&can_data.INVB_ERR_2,
    };
    AMK_init(&g_powertrain.front_left, &front_left_config);

    // Inverter C
    const AMK_config_t rear_left_config = {
        .set_function = flush_invc,
        .crit         = (INVA_CRIT_data_t *)&can_data.INVC_CRIT,
        .info         = (INVA_INFO_data_t *)&can_data.INVC_INFO,
        .temps        = (INVA_TEMPS_data_t *)&can_data.INVC_TEMPS,
        .err1         = (INVA_ERR_1_data_t *)&can_data.INVC_ERR_1,
        .err2         = (INVA_ERR_2_data_t *)&can_data.INVC_ERR_2,
    };
    AMK_init(&g_powertrain.rear_left, &rear_left_config);

    // Inverter D
    const AMK_config_t rear_right_config = {
        .set_function = flush_invd,
        .crit         = (INVA_CRIT_data_t *)&can_data.INVD_CRIT,
        .info         = (INVA_INFO_data_t *)&can_data.INVD_INFO,
        .temps        = (INVA_TEMPS_data_t *)&can_data.INVD_TEMPS,
        .err1         = (INVA_ERR_1_data_t *)&can_data.INVD_ERR_1,
        .err2         = (INVA_ERR_2_data_t *)&can_data.INVD_ERR_2,
    };
    AMK_init(&g_powertrain.rear_right, &rear_right_config);
}

void powertrain_periodic(void) {
    // stage torque requests to the AMKs
    AMK_set_torque(&g_powertrain.front_right, g_powertrain.torque_request.front_right);
    AMK_set_torque(&g_powertrain.front_left,  g_powertrain.torque_request.front_left);
    AMK_set_torque(&g_powertrain.rear_left,   g_powertrain.torque_request.rear_left);
    AMK_set_torque(&g_powertrain.rear_right,  g_powertrain.torque_request.rear_right);

    // iterate the AMK fsms and flush the setpoints to the CAN bus
    bool is_precharge_complete = is_clear(FAULT_ID_PRECHARGE_INCOMPLETE);
    AMK_periodic(&g_powertrain.front_right, is_precharge_complete);
    AMK_periodic(&g_powertrain.front_left, is_precharge_complete);
    AMK_periodic(&g_powertrain.rear_left, is_precharge_complete);
    AMK_periodic(&g_powertrain.rear_right, is_precharge_complete);
}

void powertrain_set_torque_request(torque_request_t request) {
    g_powertrain.torque_request = request;
}

bool is_powertrain_ready(void) {
    return g_powertrain.front_right.state == AMK_STATE_RUNNING
        && g_powertrain.front_left.state  == AMK_STATE_RUNNING
        && g_powertrain.rear_left.state   == AMK_STATE_RUNNING
        && g_powertrain.rear_right.state  == AMK_STATE_RUNNING;
}
