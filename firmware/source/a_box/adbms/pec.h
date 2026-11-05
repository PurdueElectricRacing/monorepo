/**
 * @file pec.h
 * @brief Calculation of PEC (CRC15, CRC10) for ADBMS communication.
 *
 * @author Analog Devices
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PEC_H
#define PEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint16_t PEC_calc_pec15(const uint8_t *data, uint8_t len);
uint16_t PEC_calc_pec10(const uint8_t *data_buf, size_t length, bool is_rx_cmd);

#endif // PEC_H
