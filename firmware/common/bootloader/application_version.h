/**
 * @file application_version.h
 * @brief Version values reported by bootloader-aware applications.
 */

#ifndef PER_APPLICATION_VERSION_H
#define PER_APPLICATION_VERSION_H

#include <stdbool.h>

#if defined(BOOTLOADER_ENABLED)
#define APPLICATION_BOOTLOADABLE true
#else
#define APPLICATION_BOOTLOADABLE false
#endif

#endif /* PER_APPLICATION_VERSION_H */
