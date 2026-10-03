/**
 * @file faults.c
 * @brief Faults page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "faults.h"

#include "can_library/generated/fault_data.h"
#include "common/nextion/nextion.h"
#include "colors.h"

static char *const fault_text_objects[DISPLAY_FAULT_COUNT] = {
    [DISPLAY_FAULT_0] = FAULT1_TXT,
    [DISPLAY_FAULT_1] = FAULT2_TXT,
    [DISPLAY_FAULT_2] = FAULT3_TXT,
    [DISPLAY_FAULT_3] = FAULT4_TXT,
    [DISPLAY_FAULT_4] = FAULT5_TXT,
    [DISPLAY_FAULT_5] = FAULT6_TXT,
    [DISPLAY_FAULT_6] = FAULT7_TXT,
    [DISPLAY_FAULT_7] = FAULT8_TXT,
};

static fault_id_t selected[DISPLAY_FAULT_COUNT];
static uint8_t selected_count;

static void update_selected_faults(void) {
    selected_count = 0;

    for (fault_id_t id = 0; id < TOTAL_NUM_FAULTS; ++id) {
        if (!is_latched(id)) {
            continue; // skip non-latched faults
        }

        uint8_t position = 0;
        // Keep equal-priority faults in fault-ID order.
        while (position < selected_count && faults[selected[position]].priority >= faults[id].priority) {
            ++position;
        }
        if (position == DISPLAY_FAULT_COUNT) {
            continue;
        }

        if (selected_count < DISPLAY_FAULT_COUNT) {
            ++selected_count;
        }
        for (uint8_t index = selected_count - 1; index > position; --index) {
            selected[index] = selected[index - 1];
        }
        selected[position] = id;
    }
}

void faults_telemetry_update(void) {
    update_selected_faults();

    for (uint8_t index = 0; index < selected_count; ++index) {
        const char *text = get_fault_string(selected[index]);
        NXT_setTextFormatted(fault_text_objects[index], "%s", text);
        NXT_setFontColor(fault_text_objects[index], RED);
    }

    for (uint8_t index = selected_count; index < DISPLAY_FAULT_COUNT; ++index) {
        NXT_setText(fault_text_objects[index], FAULT_NONE_STRING);
        NXT_setFontColor(fault_text_objects[index], WHITE);
    }
}
