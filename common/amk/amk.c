/**
 * @file amk.c
 * @brief Modernized AMK driver
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Cole Roberts (rober638@purdue.edu)
 * @author Chris McGalliard (cpmcgalliard@gmail.com)
 */

#include "common/amk/amk.h"

// Diagnostic IDs
static constexpr uint32_t AMK_CAN_ERR_ID = 3587U;
static constexpr uint32_t AMK_DC_BUS_ID  = 1049U;

// Default limits in ppt (parts per thousand)
static constexpr int16_t AMK_DEFAULT_POS_LIMIT = 2140;
static constexpr int16_t AMK_DEFAULT_NEG_LIMIT = -1000;

void AMK_init(
    AMK_t *amk,
    void (*set_function)(void),
    INVA_SET_data_t *set,
    INVA_CRIT_data_t *crit,
    INVA_INFO_data_t *info,
    INVA_TEMPS_data_t *temps,
    INVA_ERR_1_data_t *err1,
    INVA_ERR_2_data_t *err2,
    bool *precharge_ptr
) {
    amk->next_state    = AMK_STATE_OFF;
    amk->state         = AMK_STATE_OFF;
    amk->set_function  = set_function;
    amk->set           = set;
    amk->crit          = crit;
    amk->info          = info;
    amk->temps         = temps;
    amk->err1          = err1;
    amk->err2          = err2;
    amk->precharge_ptr = precharge_ptr;

    // explicitly set all control flags to safe defaults
    amk->set->AMK_Control_bDcOn       = false;
    amk->set->AMK_Control_bInverterOn = false;
    amk->set->AMK_Control_bEnable     = false;
    amk->set->AMK_Control_bErrorReset = false;
    amk->set->AMK_TorqueSetpoint      = 0;
    amk->set->AMK_PositiveTorqueLimit = AMK_DEFAULT_POS_LIMIT;
    amk->set->AMK_NegativeTorqueLimit = AMK_DEFAULT_NEG_LIMIT;
}

void AMK_reset(AMK_t *amk) {
    amk->set->AMK_Control_bErrorReset = true;
    amk->set->AMK_Control_bInverterOn = false;
    amk->set->AMK_TorqueSetpoint      = 0;
}

void AMK_set_torque(AMK_t *amk, int16_t torque_percent) {
    if (amk->state != AMK_STATE_RUNNING) {
        return;
    }

    if (torque_percent > 210)
        torque_percent = 210;

    // Scale to ppt (parts per thousand)
    amk->set->AMK_TorqueSetpoint = torque_percent * 10;
}

static void AMK_stop(AMK_t *amk) {
    amk->set->AMK_TorqueSetpoint      = 0;
    amk->set->AMK_PositiveTorqueLimit = 0;
    amk->set->AMK_NegativeTorqueLimit = 0;
    amk->set->AMK_Control_bDcOn       = false;
    amk->set->AMK_Control_bInverterOn = false;
    amk->set->AMK_Control_bEnable     = false;
}

void AMK_periodic(AMK_t *amk) {
    amk->state      = amk->next_state;
    amk->next_state = amk->state; // default: stay in current state

    bool is_system_ready = *(amk->precharge_ptr) && amk->info->AMK_Status_bSystemReady;
    bool is_error        = amk->info->AMK_Status_bError;
    bool is_simple_error = (amk->err1->AMK_DiagnosticNumber == AMK_CAN_ERR_ID
                            || amk->err1->AMK_DiagnosticNumber == AMK_DC_BUS_ID);

    bool is_DcOn_acknowledged       = amk->info->AMK_Status_bQuitDcOn;
    bool is_InverterOn_acknowledged = amk->info->AMK_Status_bQuitInverterOn;

    switch (amk->state) {
        case AMK_STATE_OFF:
            AMK_stop(amk);

            if (is_error && is_simple_error) {
                amk->next_state = AMK_STATE_RECOVERING;
                break;
            }
            
            if (!is_error && is_system_ready) {
                amk->next_state = AMK_STATE_STARTING;
            }
            break;

        case AMK_STATE_STARTING:
            amk->set->AMK_TorqueSetpoint      = 0;
            amk->set->AMK_PositiveTorqueLimit = 0;
            amk->set->AMK_NegativeTorqueLimit = 0;
            amk->set->AMK_Control_bDcOn       = true;
            amk->set->AMK_Control_bInverterOn = true;
            amk->set->AMK_Control_bEnable     = true;

            if (!is_system_ready) {
                amk->next_state = AMK_STATE_OFF;
                break;
            }

            // only transition to running once inverter is ready
            if (is_DcOn_acknowledged && is_InverterOn_acknowledged) {
                amk->next_state = AMK_STATE_RUNNING;
            }
            break;

        case AMK_STATE_RUNNING:
            amk->set->AMK_PositiveTorqueLimit = AMK_DEFAULT_POS_LIMIT;
            amk->set->AMK_NegativeTorqueLimit = AMK_DEFAULT_NEG_LIMIT;

            if (!is_system_ready || is_error) {
                amk->next_state = AMK_STATE_OFF;
                break;
            }
            break;

        case AMK_STATE_RECOVERING:
            AMK_reset(amk);

            // todo max retry count before giving up and going to fatal
            if (!is_error) {
                amk->next_state = AMK_STATE_OFF;
            }
            break;

        case AMK_STATE_FATAL:
            AMK_stop(amk);
            amk->next_state = AMK_STATE_FATAL; // stay here forever
            break;
    }

    // flush the internal state to the CAN library
    amk->set_function();

    // clear error reset
    amk->set->AMK_Control_bErrorReset = false;
}
