/**
 * @file commands.c
 * @brief Listing of all ADBMS commands.
 *
 * @author Analog Devices
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "commands.h"

#include <stdint.h>

// Register configuration
const uint8_t WRCFGA[2] = {0x00, 0x01};
const uint8_t WRCFGB[2] = {0x00, 0x24};
const uint8_t RDCFGA[2] = {0x00, 0x02};
const uint8_t RDCFGB[2] = {0x00, 0x26};

// Read cell voltage result registers
const uint8_t RDCVA[2]   = {0x00, 0x04};
const uint8_t RDCVB[2]   = {0x00, 0x06};
const uint8_t RDCVC[2]   = {0x00, 0x08};
const uint8_t RDCVD[2]   = {0x00, 0x0A};
const uint8_t RDCVE[2]   = {0x00, 0x09};
const uint8_t RDCVF[2]   = {0x00, 0x0B};
const uint8_t RDCVALL[2] = {0x00, 0x0C};

// Read average cell voltage result registers
const uint8_t RDACA[2]   = {0x00, 0x44};
const uint8_t RDACB[2]   = {0x00, 0x46};
const uint8_t RDACC[2]   = {0x00, 0x48};
const uint8_t RDACD[2]   = {0x00, 0x4A};
const uint8_t RDACE[2]   = {0x00, 0x49};
const uint8_t RDACF[2]   = {0x00, 0x4B};
const uint8_t RDACALL[2] = {0x00, 0x4C};

// Read S voltage result registers
const uint8_t RDSVA[2]  = {0x00, 0x03};
const uint8_t RDSVB[2]  = {0x00, 0x05};
const uint8_t RDSVC[2]  = {0x00, 0x07};
const uint8_t RDSVD[2]  = {0x00, 0x0D};
const uint8_t RDSVE[2]  = {0x00, 0x0E};
const uint8_t RDSVF[2]  = {0x00, 0x0F};
const uint8_t RDSALL[2] = {0x00, 0x10};

// Read C and S results
const uint8_t RDCSALL[2]  = {0x00, 0x11};
const uint8_t RDACSALL[2] = {0x00, 0x51};

// Read all AUX and all status registers
const uint8_t RDASALL[2] = {0x00, 0x35};

// Read filtered cell voltage result registers
const uint8_t RDFCA[2]   = {0x00, 0x12};
const uint8_t RDFCB[2]   = {0x00, 0x13};
const uint8_t RDFCC[2]   = {0x00, 0x14};
const uint8_t RDFCD[2]   = {0x00, 0x15};
const uint8_t RDFCE[2]   = {0x00, 0x16};
const uint8_t RDFCF[2]   = {0x00, 0x17};
const uint8_t RDFCALL[2] = {0x00, 0x18};

// Read aux results
const uint8_t RDAUXA[2] = {0x00, 0x19};
const uint8_t RDAUXB[2] = {0x00, 0x1A};
const uint8_t RDAUXC[2] = {0x00, 0x1B};
const uint8_t RDAUXD[2] = {0x00, 0x1F};

// Read redundant aux results
const uint8_t RDRAXA[2] = {0x00, 0x1C};
const uint8_t RDRAXB[2] = {0x00, 0x1D};
const uint8_t RDRAXC[2] = {0x00, 0x1E};
const uint8_t RDRAXD[2] = {0x00, 0x25};

// Read status registers
const uint8_t RDSTATA[2]    = {0x00, 0x30};
const uint8_t RDSTATB[2]    = {0x00, 0x31};
const uint8_t RDSTATC[2]    = {0x00, 0x32};
const uint8_t RDSTATCERR[2] = {0x00, 0x72};
const uint8_t RDSTATD[2]    = {0x00, 0x33};
const uint8_t RDSTATE[2]    = {0x00, 0x34};

// PWM registers commands
const uint8_t WRPWMA[2] = {0x00, 0x20};
const uint8_t RDPWMA[2] = {0x00, 0x22};

const uint8_t WRPWMB[2] = {0x00, 0x21};
const uint8_t RDPWMB[2] = {0x00, 0x23};

// Clear commands
const uint8_t CLRCELL[2] = {0x07, 0x11};
const uint8_t CLRAUX[2]  = {0x07, 0x12};
const uint8_t CLRSPIN[2] = {0x07, 0x16};
const uint8_t CLRFLAG[2] = {0x07, 0x17};
const uint8_t CLRFC[2]   = {0x07, 0x14};
const uint8_t CLOVUV[2]  = {0x07, 0x15};

// Poll ADC commands
const uint8_t PLADC[2]  = {0x07, 0x18};
const uint8_t PLAUT[2]  = {0x07, 0x19};
const uint8_t PLCADC[2] = {0x07, 0x1C};
const uint8_t PLSADC[2] = {0x07, 0x1D};
const uint8_t PLAUX1[2] = {0x07, 0x1E};
const uint8_t PLAUX2[2] = {0x07, 0x1F};

// Diagnostic command
const uint8_t DIAGN[2] = {0x07, 0x15};

// GPIOs commands
const uint8_t WRCOMM[2] = {0x07, 0x21};
const uint8_t RDCOMM[2] = {0x07, 0x22};
const uint8_t STCOMM[13] =
    {0x07, 0x23, 0xB9, 0xE4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Mute and Unmute commands
const uint8_t MUTE[2]   = {0x00, 0x28};
const uint8_t UNMUTE[2] = {0x00, 0x29};

const uint8_t RSTCC[2]  = {0x00, 0x2E};
const uint8_t SNAP[2]   = {0x00, 0x2D};
const uint8_t UNSNAP[2] = {0x00, 0x2F};
const uint8_t SRST[2]   = {0x00, 0x27};

// Read SID command
const uint8_t RDSID[2] = {0x00, 0x2C};