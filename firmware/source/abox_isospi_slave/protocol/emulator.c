#include "emulator.h"
#include "commands.h"
#include "pec.h"
#include <string.h>

static void response_pec(uint8_t *packet) {
    packet[6] = 0; /* Fixed zero command counter, included in received-data PEC. */
    uint16_t pec = adbms_pec_get_pec10(true, 6, packet);
    packet[6] = (uint8_t)(pec >> 8);
    packet[7] = (uint8_t)pec;
}
void emulator_begin(emulator_t *e) {
    e->position = 0;
    e->response = NULL;
    e->write_bank = -1;
    e->valid = false;
    e->recognized = false;
    e->write_valid = true;
}
void emulator_init(emulator_t *e) {
    memset(e, 0, sizeof(*e));
    for (size_t bank = 0; bank < 2; ++bank) {
        for (size_t module = 0; module < EMU_MODULES; ++module) {
            response_pec(&e->config[bank][module * EMU_PACKET_SIZE]);
        }
    }
    for (size_t group = 0; group < 10; ++group) {
        for (size_t module = 0; module < EMU_MODULES; ++module) {
            uint8_t *packet = &e->measurements[group][module * EMU_PACKET_SIZE];
            for (size_t channel = 0; channel < 3; ++channel) {
                size_t index = (group < 6 ? group : group - 6) * 3 + channel;
                /* raw 14667 -> 3.70005 V; raw 0 -> 1.5 V. */
                uint16_t raw = (uint16_t)((group < 6 ? 14667U : 0U) + module * 20U + index * 3U);
                if (index >= (group < 6 ? 16U : 10U)) { raw = 0; }
                packet[channel * 2] = (uint8_t)raw;
                packet[channel * 2 + 1] = (uint8_t)(raw >> 8);
            }
            response_pec(packet);
        }
    }
    emulator_begin(e);
}
static bool command_is(const emulator_t *e, const uint8_t *cmd) {
    return e->header[0] == cmd[0] && e->header[1] == cmd[1];
}
static void select_command(emulator_t *e) {
    const uint8_t *const reads[] = {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF,
                                   RDAUXA, RDAUXB, RDAUXC, RDAUXD};
    e->command_pec = adbms_pec_get_pec15(2, e->header);
    e->recognized = true;
    if (command_is(e, WRCFGA)) { e->write_bank = 0; }
    else if (command_is(e, WRCFGB)) { e->write_bank = 1; }
    else if (command_is(e, RDCFGA)) { e->response = e->config[0]; }
    else if (command_is(e, RDCFGB)) { e->response = e->config[1]; }
    else {
        for (size_t i = 0; i < 10; ++i) {
            if (command_is(e, reads[i])) { e->response = e->measurements[i]; return; }
        }
        /* Exact conversion commands emitted by the copied abox driver. */
        e->recognized = (e->header[0] == 0x02 && e->header[1] == 0xE4) ||
                        (e->header[0] == 0x04 && e->header[1] == 0x10);
    }
}
uint8_t emulator_byte(emulator_t *e, uint8_t rx) {
    size_t pos = e->position;
    /* Saturate after the maximum frame; extra clocks only shift dummy data. */
    if (pos < 4U + EMU_RESPONSE_SIZE + 1U) { ++e->position; }
    if (pos < 4) {
        e->header[pos] = rx;
        if (pos == 1) { select_command(e); }
        if (pos == 3) {
            uint16_t received = (uint16_t)((uint16_t)e->header[2] << 8 | rx);
            if (received != e->command_pec) { ++e->stats.invalid_pec; }
            else if (!e->recognized) { ++e->stats.unsupported; }
            else { e->valid = true; ++e->stats.commands; }
        }
    } else if (e->valid && e->write_bank >= 0 && pos < 4U + EMU_RESPONSE_SIZE) {
        size_t offset = pos - 4U;
        e->write_buffer[offset] = rx;
        if (offset % EMU_PACKET_SIZE == 7) {
            uint8_t *packet = &e->write_buffer[offset - 7];
            uint16_t received = (uint16_t)((uint16_t)packet[6] << 8 | packet[7]);
            if (received != adbms_pec_get_pec10(false, 6, packet)) {
                ++e->stats.invalid_pec;
                e->write_valid = false;
            }
        }
        /* Commit only complete, valid chain writes. Reads use the reverse order. */
        if (offset == EMU_RESPONSE_SIZE - 1U && e->write_valid) {
            for (size_t i = 0; i < EMU_MODULES; ++i) {
                uint8_t *dest = &e->config[e->write_bank][i * EMU_PACKET_SIZE];
                memcpy(dest, &e->write_buffer[(EMU_MODULES - 1U - i) * EMU_PACKET_SIZE], 6);
                response_pec(dest);
            }
        }
    }
    if (e->valid && e->response && e->position >= 4 && e->position < 4U + EMU_RESPONSE_SIZE) {
        return e->response[e->position - 4U];
    }
    return 0;
}
void emulator_end(emulator_t *e) {
    if ((e->position > 0 && e->position < 4) ||
        (e->valid && (e->response || e->write_bank >= 0) && e->position < 4U + EMU_RESPONSE_SIZE)) {
        ++e->stats.incomplete;
    }
    emulator_begin(e);
}
