#ifndef CONFIG_H
#define CONFIG_H
/**
 * @file driveline_config.h
 * @brief "Driveline" configuration definitions
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

// ! IS_FRONT_DRIVELINE and IS_REAR_DRIVELINE is defined by the CMake, do not define it manually here

// Check to prevent both being defined at the same time
#if defined(IS_FRONT_DRIVELINE) && defined(IS_REAR_DRIVELINE)
#error "IS_FRONT_DRIVELINE and IS_REAR_DRIVELINE cannot both be defined"
#endif

// Check to prevent neither being defined
#if !defined(IS_FRONT_DRIVELINE) && !defined(IS_REAR_DRIVELINE)
#error "Either IS_FRONT_DRIVELINE or IS_REAR_DRIVELINE must be defined"
#endif

#ifdef IS_FRONT_DRIVELINE
#define SEND_SHOCKPOTS CAN_SEND_front_shockpots
#define SEND_OIL_TEMPS CAN_SEND_front_oil_temps
#define SEND_INIT CAN_SEND_front_driveline_init
#define SEND_VERSION CAN_SEND_front_driveline_version
#endif

#ifdef IS_REAR_DRIVELINE
#define SEND_SHOCKPOTS CAN_SEND_rear_shockpots
#define SEND_OIL_TEMPS CAN_SEND_rear_oil_temps
#define SEND_INIT CAN_SEND_rear_driveline_init
#define SEND_VERSION CAN_SEND_rear_driveline_version
#endif

#endif // CONFIG_H