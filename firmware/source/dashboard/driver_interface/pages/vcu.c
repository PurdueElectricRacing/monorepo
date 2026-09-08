/**
 * @file vcu.c
 * @brief VCU page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "vcu.h"
#include "can_library/generated/DASHBOARD.h"
#include "common/nextion/nextion.h"
#include "common/utils/clamp.h"
#include "lcd.h"
#include "menu_system.h"

typedef enum {
    VCU_MODE_INDEX     = 0,
    LATERAL_GAIN_INDEX = 1,
    LONG_GAIN_INDEX    = 2,
    EBB_INDEX          = 3,
    REGEN_INDEX        = 4,
    TV_INDEX           = 5,
    LEFT_WHEEL_INDEX   = 6,
    RIGHT_WHEEL_INDEX  = 7,
    NUM_VCU_ELEMENTS
} vcu_elements_t;

static char *const VCU_MODE_LABELS[] = {
    [VCU_MODE_ACCEL]     = "ACCEL",
    [VCU_MODE_SKIDPAD]   = "SKIDPAD",
    [VCU_MODE_AUTOCROSS] = "AUTOCROSS",
    [VCU_MODE_ENDURANCE] = "ENDURANCE",
    [VCU_MODE_TUNING]    = "TUNING"
};

static char *const VCU_BINDING_LABELS[] = {
    [VCU_BINDING_LATERAL_GAIN]       = "LAT GAIN",
    [VCU_BINDING_LONGITUDINAL_GAIN]  = "LONG GAIN",
    [VCU_BINDING_EBB]                = "EBB",
};

static char *const ON_OFF_LABELS[] = {
    [0] = "OFF",
    [1] = "ON",
};

static void render_vcu_element(menu_element_t* element) {
    switch (element->type) {
        case ELEMENT_VAL:
            if (element->labels != nullptr) {
                NXT_setText(element->object_name, element->labels[element->current_value]);
            } else {
                NXT_setTextFormatted(element->object_name, "%d", element->current_value);
            }
            break;
        case ELEMENT_OPTION:
        case ELEMENT_FLT:
            NXT_setValue(element->object_name, element->current_value);
            break;
        default:
            break;
    }
}

static uint8_t binding_to_element_index(vcu_binding_t binding) {
    switch (binding) {
        case VCU_BINDING_LATERAL_GAIN:
            return LATERAL_GAIN_INDEX;
        case VCU_BINDING_LONGITUDINAL_GAIN:
            return LONG_GAIN_INDEX;
        case VCU_BINDING_EBB:
        default:
            return EBB_INDEX;
    }
}

static menu_element_t vcu_elements[NUM_VCU_ELEMENTS] = {
    [VCU_MODE_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = VCU_MODE_BUTTON,
        .labels        = VCU_MODE_LABELS,
        .current_value = VCU_MODE_ACCEL,
        .increment     = 1,
        .min_value     = VCU_MODE_ACCEL,
        .max_value     = VCU_MODE_TUNING,
        .on_change     = send_vcu_driver_request
    },
    [LATERAL_GAIN_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = LATERAL_GAIN_BUTTON,
        .current_value = 50,
        .increment     = 1,
        .min_value     = 0,
        .max_value     = 100,
        .on_change     = send_vcu_driver_request
    },
    [LONG_GAIN_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = LONG_GAIN_BUTTON,
        .current_value = 50,
        .increment     = 1,
        .min_value     = 0,
        .max_value     = 100,
        .on_change     = send_vcu_driver_request
    },
    [EBB_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = EBB_BUTTON,
        .current_value = 50,
        .increment     = 1,
        .min_value     = 0,
        .max_value     = 100,
        .on_change     = send_vcu_driver_request
    },
    [REGEN_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = REGEN_BUTTON,
        .labels        = ON_OFF_LABELS,
        .current_value = 0,
        .increment     = 1,
        .min_value     = 0,
        .max_value     = 1,
        .on_change     = send_vcu_driver_request
    },
    [TV_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = TV_BUTTON,
        .labels        = ON_OFF_LABELS,
        .current_value = 0,
        .increment     = 1,
        .min_value     = 0,
        .max_value     = 1,
        .on_change     = send_vcu_driver_request
    },
    [LEFT_WHEEL_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = LEFT_WHEEL_BUTTON,
        .labels        = VCU_BINDING_LABELS,
        .current_value = VCU_BINDING_LATERAL_GAIN,
        .increment     = 1,
        .min_value     = VCU_BINDING_LATERAL_GAIN,
        .max_value     = VCU_BINDING_EBB
    },
    [RIGHT_WHEEL_INDEX] = {
        .type          = ELEMENT_VAL,
        .object_name   = RIGHT_WHEEL_BUTTON,
        .labels        = VCU_BINDING_LABELS,
        .current_value = VCU_BINDING_EBB,
        .increment     = 1,
        .min_value     = VCU_BINDING_LATERAL_GAIN,
        .max_value     = VCU_BINDING_EBB
    }
};

menu_page_t vcu_page = {
    .elements            = vcu_elements,
    .num_elements        = NUM_VCU_ELEMENTS,
    .current_index       = 0,
    .is_element_selected = false
};

void vcu_update(void) {
    MS_refreshPage(&vcu_page);
}

void vcu_settings_CALLBACK(void) {
    if (vcu_page.is_element_selected) {
        return;
    }

    // update the values
    vcu_elements[VCU_MODE_INDEX].current_value = can_data.vcu_settings.vcu_mode;
    vcu_elements[LATERAL_GAIN_INDEX].current_value = can_data.vcu_settings.lateral_gain;
    vcu_elements[LONG_GAIN_INDEX].current_value = can_data.vcu_settings.longitudinal_gain;
    vcu_elements[EBB_INDEX].current_value = can_data.vcu_settings.electronic_brake_bias;
    vcu_elements[REGEN_INDEX].current_value = can_data.vcu_settings.is_regen_enabled;
    vcu_elements[TV_INDEX].current_value = can_data.vcu_settings.is_tv_enabled;

    if (curr_page == PAGE_VCU) { // refresh the values
        render_vcu_element(&vcu_elements[VCU_MODE_INDEX]);
        render_vcu_element(&vcu_elements[LATERAL_GAIN_INDEX]);
        render_vcu_element(&vcu_elements[LONG_GAIN_INDEX]);
        render_vcu_element(&vcu_elements[EBB_INDEX]);
        render_vcu_element(&vcu_elements[REGEN_INDEX]);
        render_vcu_element(&vcu_elements[TV_INDEX]);
    }
}

void vcu_move_up() {
    MS_moveUp(&vcu_page);
}

void vcu_move_down() {
    MS_moveDown(&vcu_page);
}

void vcu_select() {
    MS_select(&vcu_page);
}

void vcu_wheel_adjust(bool is_right_wheel, int8_t delta) {
    if (delta == 0) {
        return;
    }

    const uint8_t binding_index = is_right_wheel ? RIGHT_WHEEL_INDEX : LEFT_WHEEL_INDEX;
    const vcu_binding_t binding = (vcu_binding_t)vcu_elements[binding_index].current_value;
    const uint8_t target_index = binding_to_element_index(binding);
    menu_element_t* target = &vcu_elements[target_index];

    target->current_value = (uint16_t)CLAMP((int32_t)target->current_value + delta,
                                            target->min_value,
                                            target->max_value);

    if (curr_page == PAGE_VCU) {
        render_vcu_element(target);
    }

    send_vcu_driver_request();
}

void vcu_toggle_regen(void) {
    vcu_elements[REGEN_INDEX].current_value ^= 1;
    if (curr_page == PAGE_VCU) {
        render_vcu_element(&vcu_elements[REGEN_INDEX]);
    }
    send_vcu_driver_request();
}

void send_vcu_driver_request(void) {
    CAN_SEND_vcu_driver_request(
        (vcu_mode_t)vcu_elements[VCU_MODE_INDEX].current_value,
        (uint8_t)vcu_elements[LATERAL_GAIN_INDEX].current_value,
        (uint8_t)vcu_elements[LONG_GAIN_INDEX].current_value,
        (uint8_t)vcu_elements[EBB_INDEX].current_value,
        (bool)vcu_elements[REGEN_INDEX].current_value,
        (bool)vcu_elements[TV_INDEX].current_value
    );
}
