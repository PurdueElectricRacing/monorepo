/**
 * @file bootloader_common.c
 * @brief Application reset hand-off and generated bootloader CAN callbacks.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "common/bootloader/bootloader_common.h"

#include "stm32g474xx.h"

/* CANpiler generates the declarations and can_data fields used below. */
#if defined(CAN_NODE_MAIN_MODULE)
#include "can_library/generated/MAIN_MODULE.h"
void bl_main_module_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED)
    NVIC_SystemReset();
#endif
}
#elif defined(CAN_NODE_DASHBOARD)
#include "can_library/generated/DASHBOARD.h"
void bl_dashboard_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED)
    NVIC_SystemReset();
#endif
}
#elif defined(CAN_NODE_A_BOX)
#include "can_library/generated/A_BOX.h"
void bl_a_box_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED)
    NVIC_SystemReset();
#endif
}
#elif defined(CAN_NODE_TORQUE_VECTOR)
#include "can_library/generated/TORQUE_VECTOR.h"
void bl_torque_vector_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED)
    NVIC_SystemReset();
#endif
}
#elif defined(CAN_NODE_DRIVELINE)
#include "can_library/generated/DRIVELINE.h"
void bl_front_driveline_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED) && defined(IS_FRONT_DRIVELINE)
    NVIC_SystemReset();
#endif
}
void bl_rear_driveline_start_CALLBACK(void) {
#if defined(BOOTLOADER_ENABLED) && defined(IS_REAR_DRIVELINE)
    NVIC_SystemReset();
#endif
}
#endif
