#include "emulator.h"
#include "commands.h"
#include "pec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Bit-at-a-time references, independent of the copied lookup tables. */
static uint16_t crc15(const uint8_t *p, size_t n) {
    uint16_t r = 16;
    for (size_t i = 0; i < n; ++i) {
        r ^= (uint16_t)((uint16_t)p[i] << 7);
        for (unsigned b = 0; b < 8; ++b) {
            r = (uint16_t)((r << 1) ^ ((r & 0x4000) ? 0x4599 : 0));
        }
    }
    return (uint16_t)(r << 1);
}
static uint16_t crc10(const uint8_t *p) {
    uint16_t r = 16;
    for (size_t i = 0; i < 6; ++i) {
        r ^= (uint16_t)((uint16_t)p[i] << 2);
        for (unsigned b = 0; b < 8; ++b) {
            r = (uint16_t)((r << 1) ^ ((r & 0x200) ? 0x8f : 0));
        }
    }
    for (unsigned b = 0; b < 6; ++b) {
        r = (uint16_t)((r << 1) ^ ((r & 0x200) ? 0x8f : 0));
    }
    return r & 0x3ff;
}
static uint8_t header(emulator_t *e, const uint8_t *cmd, bool bad) {
    uint16_t pec = crc15(cmd, 2);
    assert(pec == adbms_pec_get_pec15(2, cmd));
    emulator_begin(e);
    assert(emulator_byte(e, cmd[0]) == 0);
    assert(emulator_byte(e, cmd[1]) == 0);
    assert(emulator_byte(e, (uint8_t)(pec >> 8)) == 0);
    return emulator_byte(e, (uint8_t)(pec ^ (bad ? 1U : 0U)));
}
static void read_data(emulator_t *e, const uint8_t *cmd, uint8_t *data) {
    uint8_t next = header(e, cmd, false);
    assert(e->valid && e->response);
    for (size_t i = 0; i < EMU_RESPONSE_SIZE; ++i) {
        data[i] = next;
        next = emulator_byte(e, 0);
    }
    assert(next == 0);
    for (size_t i = 0; i < EMU_MODULES; ++i) {
        uint8_t *p = &data[i * 8];
        assert((p[6] & 0xfc) == 0);
        assert(crc10(p) == (uint16_t)((uint16_t)p[6] << 8 | p[7]));
        assert(adbms_pec_get_pec10(true, 6, p) == crc10(p));
    }
    emulator_end(e);
}
static void write_data(emulator_t *e, const uint8_t *cmd, bool bad, size_t length) {
    uint8_t data[EMU_RESPONSE_SIZE];
    for (size_t i = 0; i < EMU_MODULES; ++i) {
        uint8_t *p = &data[i * 8];
        for (size_t j = 0; j < 6; ++j) { p[j] = (uint8_t)(i * 11 + j); }
        uint16_t pec = crc10(p);
        assert(pec == adbms_pec_get_pec10(false, 6, p));
        p[6] = (uint8_t)(pec >> 8); p[7] = (uint8_t)pec;
    }
    if (bad) { data[7] ^= 1; }
    (void)header(e, cmd, false);
    for (size_t i = 0; i < length; ++i) { assert(emulator_byte(e, data[i]) == 0); }
    emulator_end(e);
}
int main(void) {
    emulator_t e;
    uint8_t data[EMU_RESPONSE_SIZE];
    emulator_init(&e);
    emulator_begin(&e); emulator_end(&e); /* clockless wake */
    assert(e.stats.incomplete == 0);
    const uint8_t *const writes[] = {WRCFGA, WRCFGB};
    const uint8_t *const configs[] = {RDCFGA, RDCFGB};
    for (size_t b = 0; b < 2; ++b) {
        write_data(&e, writes[b], false, EMU_RESPONSE_SIZE);
        read_data(&e, configs[b], data);
        for (size_t i = 0; i < EMU_MODULES; ++i) {
            for (size_t j = 0; j < 6; ++j) {
                assert(data[i * 8 + j] == (EMU_MODULES - 1 - i) * 11 + j);
            }
        }
    }
    const uint8_t *const reads[] = {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF,
                                   RDAUXA, RDAUXB, RDAUXC, RDAUXD};
    for (size_t g = 0; g < 10; ++g) {
        read_data(&e, reads[g], data);
        for (size_t m = 0; m < EMU_MODULES; ++m) {
            for (size_t c = 0; c < 3; ++c) {
                size_t index = (g < 6 ? g : g - 6) * 3 + c;
                unsigned expected = (unsigned)((g < 6 ? 14667U : 0U) + m * 20 + index * 3);
                if (index >= (g < 6 ? 16U : 10U)) { expected = 0; }
                uint8_t *p = &data[m * 8 + c * 2];
                assert(((unsigned)p[0] | (unsigned)p[1] << 8) == expected);
            }
        }
    }
    assert(e.stats.incomplete == 0 && e.stats.invalid_pec == 0);
    assert(header(&e, RDCFGA, true) == 0);
    for (size_t i = 0; i < 100; ++i) { assert(emulator_byte(&e, 0) == 0); }
    emulator_end(&e);
    assert(e.stats.invalid_pec == 1);
    const uint8_t unknown[] = {0xff, 0xff};
    assert(header(&e, unknown, false) == 0);
    assert(!e.valid && e.stats.unsupported == 1);
    emulator_end(&e);
    const uint8_t adcv[] = {0x02, 0xe4}, adax[] = {0x04, 0x10};
    (void)header(&e, adcv, false); assert(e.valid); emulator_end(&e);
    (void)header(&e, adax, false); assert(e.valid); emulator_end(&e);
    /* Bad/short writes must not change the cached configuration. */
    uint8_t saved[sizeof(e.config)]; memcpy(saved, e.config, sizeof(saved));
    write_data(&e, WRCFGA, true, EMU_RESPONSE_SIZE);
    assert(e.stats.invalid_pec == 2 && memcmp(saved, e.config, sizeof(saved)) == 0);
    write_data(&e, WRCFGB, false, 23);
    assert(e.stats.incomplete == 1 && memcmp(saved, e.config, sizeof(saved)) == 0);
    emulator_begin(&e); (void)emulator_byte(&e, 0); emulator_end(&e);
    assert(e.stats.incomplete == 2);
    (void)header(&e, RDCVA, false); (void)emulator_byte(&e, 0); emulator_end(&e);
    assert(e.stats.incomplete == 3);
    read_data(&e, RDCVA, data); /* Recovery after all rejected/short transactions. */
    assert(e.stats.incomplete == 3);
    puts("isoSPI emulator tests passed");
}
