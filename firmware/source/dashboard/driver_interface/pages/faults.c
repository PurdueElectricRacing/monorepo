/**
 * @file faults.c
 * @brief Faults page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "faults.h"

#include "can_library/generated/fault_data.h"
#include "common/nextion/nextion.h"

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

void faults_telemetry_update() {
    fault_id_t selected[DISPLAY_FAULT_COUNT];
    uint8_t count = 0;

    for (fault_id_t id = 0; id < TOTAL_NUM_FAULTS; ++id) {
        if (!is_latched(id)) {
            continue; // skip non-latched faults
        }

        uint8_t position = 0;
        // Keep equal-priority faults in fault-ID order.
        while (position < count && faults[selected[position]].priority >= faults[id].priority) {
            ++position;
        }
        if (position == DISPLAY_FAULT_COUNT) {
            continue;
        }

        if (count < DISPLAY_FAULT_COUNT) {
            ++count;
        }
        for (uint8_t index = count - 1; index > position; --index) {
            selected[index] = selected[index - 1];
        }
        selected[position] = id;
    }

    for (uint8_t index = 0; index < DISPLAY_FAULT_COUNT; ++index) {
        const char *text = index < count ? get_fault_string(selected[index]) : FAULT_NONE_STRING;
        NXT_setTextFormatted(fault_text_objects[index], "%s", text);
    }
}
