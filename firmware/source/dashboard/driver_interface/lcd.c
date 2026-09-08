/**
 * @file lcd.c
 * @brief LCD display management
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "lcd.h"

#include <stddef.h>
#include <stdint.h>

#include "common/nextion/nextion.h"
#include "common/phal_G4/usart/usart.h"

// pages
#include "pages/race.h"
#include "pages/calibration.h"
#include "pages/faults.h"
#include "pages/amk.h"
#include "pages/vcu.h"

volatile page_t curr_page = PAGE_PREFLIGHT; // Current page displayed on the LCD
volatile page_t prev_page = PAGE_PREFLIGHT; // Previous page displayed on the LCD

// Page handlers array stored in flash
const page_handler_t page_handlers[NUM_PAGES] = { // Order must match page_t enum
    [PAGE_RACE] = {
        .update    = nullptr,
        .move_up   = nullptr,
        .move_down = nullptr,
        .select    = nullptr,
        .telemetry = race_telemetry_update,
        .string    = RACE_STRING
    },
    [PAGE_FAULTS] = {
        .update    = faults_update,
        .move_up   = faults_move_up,
        .move_down = faults_move_down,
        .select    = faults_select,
        .telemetry = faults_telemetry_update,
        .string    = FAULT_STRING
    },
    [PAGE_CALIBRATION] = {
        .update    = nullptr,
        .move_up   = nullptr,
        .move_down = nullptr,
        .select    = nullptr,
        .telemetry = calibration_telemetry_update,
        .string    = CALIBRATION_STRING
    },
    [PAGE_AMK] = {
        .update    = nullptr,
        .move_up   = nullptr,
        .move_down = nullptr,
        .select    = nullptr,
        .telemetry = amk_telemetry_update,
        .string    = AMK_STRING
    },
    [PAGE_VCU] = {
        .update    = vcu_update,
        .move_up   = vcu_move_up,
        .move_down = vcu_move_down,
        .select    = vcu_select,
        .telemetry = nullptr,
        .string    = VCU_STRING
    }
};

// Communication queues
ALLOCATE_STRBUF(lcd_tx_buf, 2048);

// Initialize the LCD screen
// Preflight will be shown on power on, then reset to RACE
void LCD_init(uint32_t baud_rate) {
    NXT_setBaud(baud_rate);
    NXT_setBrightness(100);

    // Set page (leave preflight)
    curr_page = PAGE_RACE;
    updatePage();
}

/**
 * @brief Called periodically to send commands to the Nextion LCD display via USART
 */
void LCD_tx_update(void) {
    if (PHAL_USART_txBusy(LCD_USART)) {
        return;
    }

    if (lcd_tx_buf.length == 0) {
        return;
    }

    PHAL_USART_tx(LCD_USART, (uint8_t *)lcd_tx_buf.data, (uint16_t)lcd_tx_buf.length);
    strbuf_clear(&lcd_tx_buf);
}

/**
 * @brief Advances to the next selectable page
 */
void advancePage() {
    curr_page = curr_page + 1;
    if (curr_page >= NUM_PAGES) {
        curr_page = 0;
    }

    updatePage();
}

/**
 * @brief Moves to the previous selectable page
 */
void backPage() {
    if (curr_page == 0) {
        curr_page = NUM_PAGES - 1;
    } else {
        curr_page = curr_page - 1;
    }

    updatePage();
}

/**
 * @brief Updates LCD display page
 *
 * Key behaviors:
 * - Maintains display of error pages when active
 */
void updatePage() {
    // If we do not detect a page update, do nothing
    if (curr_page == prev_page) {
        return;
    }

    // Set the page on display
    NXT_setPage(page_handlers[curr_page].string);
    prev_page = curr_page;

    // Call update handler if available
    if (page_handlers[curr_page].update != nullptr) {
        page_handlers[curr_page].update();
    }
}

void moveUp() {
    if (curr_page >= NUM_PAGES) {
        return;
    }

    if (page_handlers[curr_page].move_up != nullptr) {
        page_handlers[curr_page].move_up();
    }
}

void moveDown() {
    if (curr_page >= NUM_PAGES) {
        return;
    }

    if (page_handlers[curr_page].move_down != nullptr) {
        page_handlers[curr_page].move_down();
    }
}

void selectItem() {
    if (curr_page >= NUM_PAGES) {
        return;
    }

    if (page_handlers[curr_page].select != nullptr) {
        page_handlers[curr_page].select();
    }
}

/**
 * @brief Updates current telemetry page by calling its handler if available
 */
void updateTelemetryPages() {
    if (curr_page >= NUM_PAGES) {
        return;
    }

    if (page_handlers[curr_page].telemetry != nullptr) {
        page_handlers[curr_page].telemetry();
    }
}
