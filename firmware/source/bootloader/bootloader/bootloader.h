/**
 * @file bootloader.h
 * @brief Bare-metal STM32G4 CAN bootloader API.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * FDCAN receive interrupts enqueue frames. The main loop calls BL_poll() so
 * flash erase/program and CRC work never runs in interrupt context.
 */

#ifndef PER_G4_BOOTLOADER_H
#define PER_G4_BOOTLOADER_H

#include <stdbool.h>

/** Bound the post-READY startup handshake before normal application launch. */
#define BL_STARTUP_WINDOW_MS 500U

/**
 * @brief Configure the node-specific transport and announce readiness.
 *
 * The system clock must be configured before this function is called.
 */
void BL_init(void);

/**
 * @brief Poll the post-READY startup window for a valid START command.
 *
 * This calls BL_poll() for BL_STARTUP_WINDOW_MS after BL_init() has announced
 * READY. It returns true once START has activated an update, allowing the
 * caller to remain resident; false permits normal application validation.
 */
bool BL_waitForUpdate(void);

/**
 * @brief Process every frame currently queued by the FDCAN receive interrupt.
 *
 * Call repeatedly from main context. This function is the sole queue consumer
 * and owns all flash and CRC work; the receive ISR only copies matching frames
 * into the bounded software queue.
 */
void BL_poll(void);

/**
 * @brief Validate the committed image and transfer control to it.
 *
 * Metadata, vector-table values, and application CRC are checked before VTOR
 * and MSP are changed.
 *
 * @return false when validation fails or the application reset handler returns.
 */
bool BL_checkAndBoot(void);

#endif /* PER_G4_BOOTLOADER_H */
