/**
 * @file nextion.h
 * @brief Nextion display driver interface
 *
 * Interface for controlling Nextion display modules through serial communication.
 * Instruction set detailed here: https://nextion.tech/instruction-set/
 *
 * @author Matthew Flanagan (matthewdavidflanagan@outlook.com)
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef NEXTION_H
#define NEXTION_H

#include <stdint.h>
#include <strbuf.h>

// Serial insturcitons must be terminated with 3 bytes 0xFF
#define NXT_CMD_TERM "\xFF\xFF\xFF"
#define ASCII_OFFSET 48

// Nextion Command Strings
#define NXT_BACKGROUND_COLOR ".bco="
#define NXT_FONT_COLOR       ".pco="
#define NXT_VALUE            ".val="
#define NXT_TEXT             ".txt="
#define NXT_BORDERW          ".borderw="
#define NXT_BORDER_COLOR     ".borderc="
#define NXT_PAGE             "page "
#define NXT_BRIGHTNESS       "dims="
#define NXT_BAUD             "bauds="

// Function Prototypes
void NXT_setFloat(char* obj_name, char* param, float num, uint8_t precision);
void NXT_setValue(char* obj_name, uint16_t val);
void NXT_setBackground(char* obj_name, uint16_t val);
void NXT_setFontColor(char* obj_name, uint16_t val);
void NXT_setBorderWidth(char* obj_name, uint16_t val);
void NXT_setBorderColor(char* obj_name, uint16_t val);
void NXT_setText(char* obj_name, char* text);
void NXT_setTextFormatted(char* obj_name, const char* format, ...);
void NXT_setPage(char* page_name);

// Configuration Functions
void NXT_setBrightness(uint8_t brightness);
void NXT_setBaud(uint32_t baud);

#endif // NEXTION_H
