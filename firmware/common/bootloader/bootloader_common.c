/**
 * @file bootloader_common.c
 * @brief Application reset hand-off and generated bootloader CAN callbacks.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "common/bootloader/bootloader_common.h"

#include "common/phal_G4/phal_G4.h"

/* Survives NVIC_SystemReset() without being initialized by startup code. */
__attribute__((section(".noinit")))
BootloaderSharedMemory_t bootloader_shared_memory;

/* Generated CAN callbacks share this protocol adapter. */
static void __attribute__((unused)) bootloader_request_from_command(uint8_t command) {
#if defined(BOOTLOADER_ENABLED)
    if (command == BLCMD_START) {
        Bootloader_ResetForFirmwareDownload();
    }
#else
    (void)command;
#endif
}

/** @brief Store a one-shot download request and reset the MCU. */
void Bootloader_ResetForFirmwareDownload(void) {
    bootloader_shared_memory.magic_word = BOOTLOADER_SHARED_MEMORY_MAGIC;
    bootloader_shared_memory.reset_reason = RESET_REASON_DOWNLOAD_FW;
    NVIC_SystemReset();
}

/* CANpiler generates the declarations and can_data fields used below. */
#if defined(CAN_NODE_MAIN_MODULE)
#include "can_library/generated/MAIN_MODULE.h"
void bl_main_module_cmd_CALLBACK(void) {
    bootloader_request_from_command((uint8_t)can_data.bl_main_module_cmd.cmd);
}
#elif defined(CAN_NODE_DASHBOARD)
#include "can_library/generated/DASHBOARD.h"
void bl_dashboard_cmd_CALLBACK(void) {
    bootloader_request_from_command((uint8_t)can_data.bl_dashboard_cmd.cmd);
}
#elif defined(CAN_NODE_A_BOX)
#include "can_library/generated/A_BOX.h"
void bl_a_box_cmd_CALLBACK(void) {
    bootloader_request_from_command((uint8_t)can_data.bl_a_box_cmd.cmd);
}
#elif defined(CAN_NODE_TORQUE_VECTOR)
#include "can_library/generated/TORQUE_VECTOR.h"
void bl_torque_vector_cmd_CALLBACK(void) {
    bootloader_request_from_command((uint8_t)can_data.bl_torque_vector_cmd.cmd);
}
#elif defined(CAN_NODE_DRIVELINE)
#include "can_library/generated/DRIVELINE.h"
void bl_front_driveline_cmd_CALLBACK(void) {
#if defined(IS_FRONT_DRIVELINE)
    bootloader_request_from_command((uint8_t)can_data.bl_front_driveline_cmd.cmd);
#endif
}
void bl_rear_driveline_cmd_CALLBACK(void) {
#if defined(IS_REAR_DRIVELINE)
    bootloader_request_from_command((uint8_t)can_data.bl_rear_driveline_cmd.cmd);
#endif
}
#endif
