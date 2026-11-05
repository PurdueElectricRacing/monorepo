/**
 * @file commands.h
 * @brief Listing of all ADBMS commands.
 *
 * @author Analog Devices
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// Register configuration
static constexpr uint8_t ADBMS6380_CMD_WRCFGA[2] = {0x00, 0x01};
static constexpr uint8_t ADBMS6380_CMD_WRCFGB[2] = {0x00, 0x24};
static constexpr uint8_t ADBMS6380_CMD_RDCFGA[2] = {0x00, 0x02};
static constexpr uint8_t ADBMS6380_CMD_RDCFGB[2] = {0x00, 0x26};

// Read cell voltage result registers
static constexpr uint8_t ADBMS6380_CMD_RDCVA[2]   = {0x00, 0x04};
static constexpr uint8_t ADBMS6380_CMD_RDCVB[2]   = {0x00, 0x06};
static constexpr uint8_t ADBMS6380_CMD_RDCVC[2]   = {0x00, 0x08};
static constexpr uint8_t ADBMS6380_CMD_RDCVD[2]   = {0x00, 0x0A};
static constexpr uint8_t ADBMS6380_CMD_RDCVE[2]   = {0x00, 0x09};
static constexpr uint8_t ADBMS6380_CMD_RDCVF[2]   = {0x00, 0x0B};
static constexpr uint8_t ADBMS6380_CMD_RDCVALL[2] = {0x00, 0x0C};

// Read average cell voltage result registers
static constexpr uint8_t ADBMS6380_CMD_RDACA[2]   = {0x00, 0x44};
static constexpr uint8_t ADBMS6380_CMD_RDACB[2]   = {0x00, 0x46};
static constexpr uint8_t ADBMS6380_CMD_RDACC[2]   = {0x00, 0x48};
static constexpr uint8_t ADBMS6380_CMD_RDACD[2]   = {0x00, 0x4A};
static constexpr uint8_t ADBMS6380_CMD_RDACE[2]   = {0x00, 0x49};
static constexpr uint8_t ADBMS6380_CMD_RDACF[2]   = {0x00, 0x4B};
static constexpr uint8_t ADBMS6380_CMD_RDACALL[2] = {0x00, 0x4C};

// Read S voltage result registers
static constexpr uint8_t ADBMS6380_CMD_RDSVA[2]  = {0x00, 0x03};
static constexpr uint8_t ADBMS6380_CMD_RDSVB[2]  = {0x00, 0x05};
static constexpr uint8_t ADBMS6380_CMD_RDSVC[2]  = {0x00, 0x07};
static constexpr uint8_t ADBMS6380_CMD_RDSVD[2]  = {0x00, 0x0D};
static constexpr uint8_t ADBMS6380_CMD_RDSVE[2]  = {0x00, 0x0E};
static constexpr uint8_t ADBMS6380_CMD_RDSVF[2]  = {0x00, 0x0F};
static constexpr uint8_t ADBMS6380_CMD_RDSALL[2] = {0x00, 0x10};

// Read C and S results
static constexpr uint8_t ADBMS6380_CMD_RDCSALL[2]  = {0x00, 0x11};
static constexpr uint8_t ADBMS6380_CMD_RDACSALL[2] = {0x00, 0x51};

// Read all AUX and all status registers
static constexpr uint8_t ADBMS6380_CMD_RDASALL[2] = {0x00, 0x35};

// Read filtered cell voltage result registers
static constexpr uint8_t ADBMS6380_CMD_RDFCA[2]   = {0x00, 0x12};
static constexpr uint8_t ADBMS6380_CMD_RDFCB[2]   = {0x00, 0x13};
static constexpr uint8_t ADBMS6380_CMD_RDFCC[2]   = {0x00, 0x14};
static constexpr uint8_t ADBMS6380_CMD_RDFCD[2]   = {0x00, 0x15};
static constexpr uint8_t ADBMS6380_CMD_RDFCE[2]   = {0x00, 0x16};
static constexpr uint8_t ADBMS6380_CMD_RDFCF[2]   = {0x00, 0x17};
static constexpr uint8_t ADBMS6380_CMD_RDFCALL[2] = {0x00, 0x18};

// Read aux results
static constexpr uint8_t ADBMS6380_CMD_RDAUXA[2] = {0x00, 0x19};
static constexpr uint8_t ADBMS6380_CMD_RDAUXB[2] = {0x00, 0x1A};
static constexpr uint8_t ADBMS6380_CMD_RDAUXC[2] = {0x00, 0x1B};
static constexpr uint8_t ADBMS6380_CMD_RDAUXD[2] = {0x00, 0x1F};

// Read redundant aux results
static constexpr uint8_t ADBMS6380_CMD_RDRAXA[2] = {0x00, 0x1C};
static constexpr uint8_t ADBMS6380_CMD_RDRAXB[2] = {0x00, 0x1D};
static constexpr uint8_t ADBMS6380_CMD_RDRAXC[2] = {0x00, 0x1E};
static constexpr uint8_t ADBMS6380_CMD_RDRAXD[2] = {0x00, 0x25};

// Read status registers
static constexpr uint8_t ADBMS6380_CMD_RDSTATA[2]    = {0x00, 0x30};
static constexpr uint8_t ADBMS6380_CMD_RDSTATB[2]    = {0x00, 0x31};
static constexpr uint8_t ADBMS6380_CMD_RDSTATC[2]    = {0x00, 0x32};
static constexpr uint8_t ADBMS6380_CMD_RDSTATCERR[2] = {0x00, 0x72};
static constexpr uint8_t ADBMS6380_CMD_RDSTATD[2]    = {0x00, 0x33};
static constexpr uint8_t ADBMS6380_CMD_RDSTATE[2]    = {0x00, 0x34};

// PWM registers commands
static constexpr uint8_t ADBMS6380_CMD_WRPWMA[2] = {0x00, 0x20};
static constexpr uint8_t ADBMS6380_CMD_RDPWMA[2] = {0x00, 0x22};

static constexpr uint8_t ADBMS6380_CMD_WRPWMB[2] = {0x00, 0x21};
static constexpr uint8_t ADBMS6380_CMD_RDPWMB[2] = {0x00, 0x23};

// Clear commands
static constexpr uint8_t ADBMS6380_CMD_CLRCELL[2] = {0x07, 0x11};
static constexpr uint8_t ADBMS6380_CMD_CLRAUX[2]  = {0x07, 0x12};
static constexpr uint8_t ADBMS6380_CMD_CLRSPIN[2] = {0x07, 0x16};
static constexpr uint8_t ADBMS6380_CMD_CLRFLAG[2] = {0x07, 0x17};
static constexpr uint8_t ADBMS6380_CMD_CLRFC[2]   = {0x07, 0x14};
static constexpr uint8_t ADBMS6380_CMD_CLOVUV[2]  = {0x07, 0x15};

// Poll ADC commands
static constexpr uint8_t ADBMS6380_CMD_PLADC[2]  = {0x07, 0x18};
static constexpr uint8_t ADBMS6380_CMD_PLAUT[2]  = {0x07, 0x19};
static constexpr uint8_t ADBMS6380_CMD_PLCADC[2] = {0x07, 0x1C};
static constexpr uint8_t ADBMS6380_CMD_PLSADC[2] = {0x07, 0x1D};
static constexpr uint8_t ADBMS6380_CMD_PLAUX1[2] = {0x07, 0x1E};
static constexpr uint8_t ADBMS6380_CMD_PLAUX2[2] = {0x07, 0x1F};

// Diagnostic command
static constexpr uint8_t ADBMS6380_CMD_DIAGN[2] = {0x07, 0x15};

// GPIOs commands
static constexpr uint8_t ADBMS6380_CMD_WRCOMM[2]  = {0x07, 0x21};
static constexpr uint8_t ADBMS6380_CMD_RDCOMM[2]  = {0x07, 0x22};
static constexpr uint8_t ADBMS6380_CMD_STCOMM[13] = {
    0x07, 0x23, 0xB9, 0xE4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Mute and Unmute commands
static constexpr uint8_t ADBMS6380_CMD_MUTE[2]   = {0x00, 0x28};
static constexpr uint8_t ADBMS6380_CMD_UNMUTE[2] = {0x00, 0x29};

static constexpr uint8_t ADBMS6380_CMD_RSTCC[2]  = {0x00, 0x2E};
static constexpr uint8_t ADBMS6380_CMD_SNAP[2]   = {0x00, 0x2D};
static constexpr uint8_t ADBMS6380_CMD_UNSNAP[2] = {0x00, 0x2F};
static constexpr uint8_t ADBMS6380_CMD_SRST[2]   = {0x00, 0x27};

// Read SID command
static constexpr uint8_t ADBMS6380_CMD_RDSID[2] = {0x00, 0x2C};

#endif // COMMANDS_H
