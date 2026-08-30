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
#include <stdint.h>
#include <stddef.h>

uint16_t adbms_pec_get_pec15(uint8_t len, const uint8_t *data);
uint16_t adbms_pec_get_pec10(bool bIsRxCmd, size_t nLength, const uint8_t *pDataBuf);

#endif // PEC_H