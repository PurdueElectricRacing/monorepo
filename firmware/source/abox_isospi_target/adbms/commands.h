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
extern const uint8_t WRCFGA[2];
extern const uint8_t WRCFGB[2];
extern const uint8_t RDCFGA[2];
extern const uint8_t RDCFGB[2];

// Read cell voltage result registers
extern const uint8_t RDCVA[2];
extern const uint8_t RDCVB[2];
extern const uint8_t RDCVC[2];
extern const uint8_t RDCVD[2];
extern const uint8_t RDCVE[2];
extern const uint8_t RDCVF[2];
extern const uint8_t RDCVALL[2];

// Read average cell voltage result registers
extern const uint8_t RDACA[2];
extern const uint8_t RDACB[2];
extern const uint8_t RDACC[2];
extern const uint8_t RDACD[2];
extern const uint8_t RDACE[2];
extern const uint8_t RDACF[2];
extern const uint8_t RDACALL[2];

// Read S voltage result registers
extern const uint8_t RDSVA[2];
extern const uint8_t RDSVB[2];
extern const uint8_t RDSVC[2];
extern const uint8_t RDSVD[2];
extern const uint8_t RDSVE[2];
extern const uint8_t RDSVF[2];
extern const uint8_t RDSALL[2];

// Read C and S results
extern const uint8_t RDCSALL[2];
extern const uint8_t RDACSALL[2];

// Read all AUX and all status registers
extern const uint8_t RDASALL[2];

// Read filtered cell voltage result registers
extern const uint8_t RDFCA[2];
extern const uint8_t RDFCB[2];
extern const uint8_t RDFCC[2];
extern const uint8_t RDFCD[2];
extern const uint8_t RDFCE[2];
extern const uint8_t RDFCF[2];
extern const uint8_t RDFCALL[2];

// Read aux results
extern const uint8_t RDAUXA[2];
extern const uint8_t RDAUXB[2];
extern const uint8_t RDAUXC[2];
extern const uint8_t RDAUXD[2];

// Read redundant aux results
extern const uint8_t RDRAXA[2];
extern const uint8_t RDRAXB[2];
extern const uint8_t RDRAXC[2];
extern const uint8_t RDRAXD[2];

// Read status registers
extern const uint8_t RDSTATA[2];
extern const uint8_t RDSTATB[2];
extern const uint8_t RDSTATC[2];
extern const uint8_t RDSTATCERR[2];
extern const uint8_t RDSTATD[2];
extern const uint8_t RDSTATE[2];

// PWM registers commands
extern const uint8_t WRPWMA[2];
extern const uint8_t RDPWMA[2];

extern const uint8_t WRPWMB[2];
extern const uint8_t RDPWMB[2];

// Clear commands
extern const uint8_t CLRCELL[2];
extern const uint8_t CLRAUX[2];
extern const uint8_t CLRSPIN[2];
extern const uint8_t CLRFLAG[2];
extern const uint8_t CLRFC[2];
extern const uint8_t CLOVUV[2];

// Poll ADC commands
extern const uint8_t PLADC[2];
extern const uint8_t PLAUT[2];
extern const uint8_t PLCADC[2];
extern const uint8_t PLSADC[2];
extern const uint8_t PLAUX1[2];
extern const uint8_t PLAUX2[2];

// Diagnostic command
extern const uint8_t DIAGN[2];

// GPIOs commands
extern const uint8_t WRCOMM[2];
extern const uint8_t RDCOMM[2];
extern const uint8_t STCOMM[13];

// Mute and Unmute commands
extern const uint8_t MUTE[2];
extern const uint8_t UNMUTE[2];

extern const uint8_t RSTCC[2];
extern const uint8_t SNAP[2];
extern const uint8_t UNSNAP[2];
extern const uint8_t SRST[2];

// Read SID command
extern const uint8_t RDSID[2];

#endif // COMMANDS_H