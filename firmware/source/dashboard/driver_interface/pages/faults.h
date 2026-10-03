#ifndef FAULTS_H
#define FAULTS_H

/**
 * @file faults.h
 * @brief Faults page implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#define FAULT_STRING "fault"

// Object names for Fault View page
#define FAULT1_TXT        "ERROR1"
#define FAULT2_TXT        "ERROR2"
#define FAULT3_TXT        "ERROR3"
#define FAULT4_TXT        "ERROR4"
#define FAULT5_TXT        "ERROR5"
#define FAULT6_TXT        "ERROR6"
#define FAULT7_TXT        "ERROR7"
#define FAULT8_TXT        "ERROR8"
#define FAULT_NONE_STRING "NONE\0"

typedef enum {
    DISPLAY_FAULT_0 = 0,
    DISPLAY_FAULT_1 = 1,
    DISPLAY_FAULT_2 = 2,
    DISPLAY_FAULT_3 = 3,
    DISPLAY_FAULT_4 = 4,
    DISPLAY_FAULT_5 = 5,
    DISPLAY_FAULT_6 = 6,
    DISPLAY_FAULT_7 = 7,
    DISPLAY_FAULT_COUNT
} display_fault_index_t;

void faults_telemetry_update(void);

#endif // FAULTS_H