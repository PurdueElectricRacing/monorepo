/**
 * @file menu_system.h
 * @brief Implementation of menu system for LCD display interface
 *
 * Provides functions for managing menu navigation, element styling,
 * and user interaction with the LCD display menu system.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdint.h>

// Element types
typedef enum {
    ELEMENT_VAL    = 0, // Numeric value
    ELEMENT_FLT    = 1, // Float value
    ELEMENT_BAR    = 2, // (not supported but easy to implement)
    ELEMENT_BUTTON = 3, // Button type
    ELEMENT_OPTION = 4, // On/off toggle
    ELEMENT_LIST   = 5 // Item in a list
} element_type_t;

// Element states
typedef enum {
    STATE_NORMAL   = 0,
    STATE_HOVER    = 1,
    STATE_SELECTED = 2
} element_state_t;

// Element structure
typedef struct {
    element_type_t type;
    element_state_t state;
    char* object_name; // Nextion object name
    void (*on_change)(void); // Callback when value changes
    char *const *labels; // Optional string labels indexed by current_value
    uint16_t current_value; // Current value for numeric types or on/off state for toggles
    uint16_t min_value; // Minimum value for numeric types
    uint16_t max_value; // Maximum value for numeric types
    uint8_t increment; // Increment for numeric types
} menu_element_t;

// Page structure
typedef struct {
    menu_element_t* elements; // Array of elements
    uint8_t num_elements; // Number of elements in array
    uint8_t current_index; // Currently selected element index
    bool is_element_selected; // Is an element currently selected?
    bool saved; // Generic saved state flag
} menu_page_t;

// Navigation functions
void MS_moveUp(menu_page_t* page);
void MS_moveDown(menu_page_t* page);
void MS_select(menu_page_t* page);

// Value modification functions
void MS_incrementValue(menu_element_t* element);
void MS_decrementValue(menu_element_t* element);

void MS_refreshPage(menu_page_t* page);
int8_t MS_listGetSelected(const menu_page_t* page);

#endif // MENU_SYSTEM_H
