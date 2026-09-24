#ifndef ISOSPI_EMULATOR_H
#define ISOSPI_EMULATOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define EMU_MODULES 7U
#define EMU_PACKET_SIZE 8U
#define EMU_RESPONSE_SIZE (EMU_MODULES * EMU_PACKET_SIZE)
typedef struct {
    volatile uint32_t commands, invalid_pec, unsupported, incomplete, peripheral_errors;
} emulator_stats_t;
typedef struct {
    uint8_t config[2][EMU_RESPONSE_SIZE];
    uint8_t measurements[10][EMU_RESPONSE_SIZE];
    uint8_t header[4], write_buffer[EMU_RESPONSE_SIZE];
    const uint8_t *response;
    size_t position;
    uint16_t command_pec;
    int write_bank;
    bool valid, recognized, write_valid;
    emulator_stats_t stats;
} emulator_t;
void emulator_init(emulator_t *e);
void emulator_begin(emulator_t *e);
/* Consume one received byte; return the byte to transmit on the NEXT byte clock. */
uint8_t emulator_byte(emulator_t *e, uint8_t rx);
void emulator_end(emulator_t *e);
#endif
