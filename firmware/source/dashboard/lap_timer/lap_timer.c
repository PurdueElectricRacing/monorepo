/**
 * @file lap_timer.c
 * @brief DASHBOARD lap timer task implementations
 * 
 * @author Amruth Nadimpally (nadimpaa@purdue.edu)
 * @author Aditya Saini (saini91@purdue.edu)
 */

#include "lap_timer.h"
#include "can_library/generated/DASHBOARD.h"

// void LWS_Standard_CALLBACK(void) {
//     bool data_valid = can_data.LWS_Standard.OK && can_data.LWS_Standard.CAL && can_data.LWS_Standard.TRIM;

//     CAN_SEND_steering_angle(
//         can_data.LWS_Standard.LWS_ANGLE,
//         can_data.LWS_Standard.LWS_SPEED,
//         data_valid
//     );
// }

// task
void lap_timer_onpress() {
    bool data_valid = can_data.
}
// void report_telemetry_02hz(void) {
//     CAN_SEND_dash_version(GIT_HASH);
// }