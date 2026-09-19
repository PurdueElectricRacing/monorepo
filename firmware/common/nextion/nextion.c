/**
 * @file nextion.c
 * @brief Nextion display driver interface
 *
 * Interface for controlling Nextion display modules through serial communication.
 * Instruction set detailed here: https://nextion.tech/instruction-set/
 *
 * @author Matthew Flanagan (matthewdavidflanagan@outlook.com)
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "nextion.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

extern strbuf_t lcd_tx_buf;

/**
 * @brief Sets a float value to a specified object parameter on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param param The parameter of the object to set the float value to.
 * @param num The float value to set.
 * @param precision The number of decimal places to include in the formatted float value.
 */
void NXT_setFloat(char* obj_name, char* param, float num, uint8_t precision) {
    strbuf_printf(&lcd_tx_buf, "%s%s\"%.*f\"", obj_name, param, precision, (double)num);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets a value to a specified object on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param val The value to set for the specified object.
 */
void NXT_setValue(char* obj_name, uint16_t val) {
    strbuf_printf(&lcd_tx_buf, "%s%s%d", obj_name, NXT_VALUE, val);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the background color of a specified object on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param val The background color value to set for the specified object.
 */
void NXT_setBackground(char* obj_name, uint16_t val) {
    strbuf_printf(&lcd_tx_buf, "%s%s%d", obj_name, NXT_BACKGROUND_COLOR, val);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the font color of a specified object on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param val The font color value to set for the specified object.
 */
void NXT_setFontColor(char* obj_name, uint16_t val) {
    strbuf_printf(&lcd_tx_buf, "%s%s%d", obj_name, NXT_FONT_COLOR, val);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the border width of a specified object on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param val The border width value to set for the specified object.
 */
void NXT_setBorderWidth(char* obj_name, uint16_t val) {
    strbuf_printf(&lcd_tx_buf, "%s%s%d", obj_name, NXT_BORDERW, val);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the border color of a specified object on the Nextion display.
 *
 * @param obj_name The name of the object on the Nextion display.
 * @param val The border color value to set for the specified object.
 */
void NXT_setBorderColor(char* obj_name, uint16_t val) {
    strbuf_printf(&lcd_tx_buf, "%s%s%d", obj_name, NXT_BORDER_COLOR, val);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the text of a Nextion display object.
 *
 * @param obj_name The name of the Nextion object (e.g., "t0").
 * @param text The text to set for the specified obect.
 */
void NXT_setText(char* obj_name, char* text) {
    strbuf_printf(&lcd_tx_buf, "%s%s\"%s\"", obj_name, NXT_TEXT, text);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the text of a Nextion display object using printf-style formatting.
 *
 * @param obj_name The name of the Nextion object (e.g., "t0").
 * @param format Printf-style format string.
 * @param ... Variable arguments for format string.
 */
// todo fix this function
void NXT_setTextFormatted(char* obj_name, const char* format, ...) {
    strbuf_printf(&lcd_tx_buf, "%s%s\"", obj_name, NXT_TEXT);
    va_list args;
    va_start(args, format);
    char formatted[50];
    vsnprintf(formatted, sizeof(formatted), format, args);
    va_end(args);
    strbuf_append(&lcd_tx_buf, formatted, strlen(formatted));
    strbuf_append(&lcd_tx_buf, "\"", 1);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the current page on the Nextion display.
 *
 * @param page_name The name of the page to set on the Nextion display.
 */
void NXT_setPage(char* page_name) {
    strbuf_printf(&lcd_tx_buf, "%s%s", NXT_PAGE, page_name);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the brightness of the Nextion display.
 *
 * @param percentage The desired brightness level as a percentage (0-100).
 */
void NXT_setBrightness(uint8_t percentage) {
    if (percentage > 100)
        percentage = 100;
    strbuf_printf(&lcd_tx_buf, "%s%d", NXT_BRIGHTNESS, percentage);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}

/**
 * @brief Sets the baud rate for the Nextion display.
 *
 * @param baud The desired baud rate to set for the Nextion display.
 */
void NXT_setBaud(uint32_t baud) {
    strbuf_printf(&lcd_tx_buf, "%s%d", NXT_BAUD, baud);
    strbuf_append(&lcd_tx_buf, NXT_CMD_TERM, 3);
}
