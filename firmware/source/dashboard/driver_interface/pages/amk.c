/**
 * @file amk.c
 * @brief AMK page implementation
 *
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (sain91@purdue.edu)
 */

#include "amk.h"

#include "can_library/generated/DASHBOARD.h"
#include "nextion/nextion.h"
#include "colors.h"

typedef struct {
    char *state;
    char *error;
    char *diagnostic;
    char *on;
} AMK_objects_t;

static void update_diagnostic_display(
    AMK_objects_t objects,
    uint32_t diagnostic_number,
    AMK_state_t inverter_state,
    bool has_error,
    bool inverter_on,
    bool any_stale,
    bool is_stale
) {
    if (is_stale) {
        NXT_setFontColor(objects.state, WHITE);
        NXT_setText(objects.state, "MAIN STALE");
        NXT_setText(objects.error, "--");
        NXT_setText(objects.diagnostic, "--");
        NXT_setText(objects.on, "--");
        return;
    }

    if (any_stale) {
        NXT_setFontColor(objects.state, RED);
        NXT_setText(objects.state, "AMK STALE");
        NXT_setText(objects.error, "--");
        NXT_setText(objects.diagnostic, "--");
        NXT_setText(objects.on, "--");
        return;
    }

    switch(inverter_state) {
        case AMK_STATE_OFF:
            NXT_setText(objects.state, "OFF");
            break;
        case AMK_STATE_STARTING:
            NXT_setText(objects.state, "STARTING");
            break;
        case AMK_STATE_RUNNING:
            NXT_setText(objects.state, "RUNNING");
            break;
        case AMK_STATE_RECOVERING:
            NXT_setText(objects.state, "RECOVERING");
            break;
        case AMK_STATE_FATAL:
            NXT_setText(objects.state, "FATAL");
            break;
        default:
            NXT_setText(objects.state, "UNKNOWN");
            break;
    }

    if (has_error) {
        NXT_setFontColor(objects.error, RED);
        NXT_setText(objects.error, "ERROR");
        NXT_setTextFormatted(objects.diagnostic, "%d", diagnostic_number);
    } else {
        NXT_setFontColor(objects.error, GREEN);
        NXT_setText(objects.error, "OK");
        NXT_setText(objects.diagnostic, "NONE");
    }
    
    if (inverter_on) {
        NXT_setFontColor(objects.on, GREEN);
        NXT_setText(objects.on, "ON");
    } else {
        NXT_setFontColor(objects.on, RED);
        NXT_setText(objects.on, "OFF");
    }
}

void amk_telemetry_update() {
    update_diagnostic_display(
        (AMK_objects_t){
            .state = INVA_STATE,
            .error = INVA_ERROR,
            .diagnostic = INVA_DIAGNOSTIC,
            .on = INVA_ON
        },
        can_data.inva_diagnostics.diagnostic_number,
        can_data.inva_diagnostics.amk_state,
        can_data.inva_diagnostics.bError,
        can_data.inva_diagnostics.InverterOn,
        can_data.inva_diagnostics.is_any_stale,
        can_data.inva_diagnostics.is_stale()
    );
    update_diagnostic_display(
        (AMK_objects_t){
            .state = INVB_STATE,
            .error = INVB_ERROR,
            .diagnostic = INVB_DIAGNOSTIC,
            .on = INVB_ON
        },
        can_data.invb_diagnostics.diagnostic_number,
        can_data.invb_diagnostics.amk_state,
        can_data.invb_diagnostics.bError,
        can_data.invb_diagnostics.InverterOn,
        can_data.invb_diagnostics.is_any_stale,
        can_data.invb_diagnostics.is_stale()
    );
    update_diagnostic_display(
        (AMK_objects_t){
            .state = INVC_STATE,
            .error = INVC_ERROR,
            .diagnostic = INVC_DIAGNOSTIC,
            .on = INVC_ON
        },
        can_data.invc_diagnostics.diagnostic_number,
        can_data.invc_diagnostics.amk_state,
        can_data.invc_diagnostics.bError,
        can_data.invc_diagnostics.InverterOn,
        can_data.invc_diagnostics.is_any_stale,
        can_data.invc_diagnostics.is_stale()
    );
    update_diagnostic_display(
        (AMK_objects_t){
            .state = INVD_STATE,
            .error = INVD_ERROR,
            .diagnostic = INVD_DIAGNOSTIC,
            .on = INVD_ON
        },
        can_data.invd_diagnostics.diagnostic_number,
        can_data.invd_diagnostics.amk_state,
        can_data.invd_diagnostics.bError,
        can_data.invd_diagnostics.InverterOn,
        can_data.invd_diagnostics.is_any_stale,
        can_data.invd_diagnostics.is_stale()
    );
}