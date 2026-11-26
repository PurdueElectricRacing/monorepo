#ifndef CAN_ROUTER_H
#define CAN_ROUTER_H

/**
 * @file can_router.h
 * @brief Auto-generated system-wide CAN node inclusion router.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#if defined(CAN_NODE_A_BOX)
    #include "can_library/generated/A_BOX.h"
#elif defined(CAN_NODE_BL_A_BOX)
    #include "can_library/generated/BL_A_BOX.h"
#elif defined(CAN_NODE_BL_DASHBOARD)
    #include "can_library/generated/BL_DASHBOARD.h"
#elif defined(CAN_NODE_BL_FRONT_DRIVELINE)
    #include "can_library/generated/BL_FRONT_DRIVELINE.h"
#elif defined(CAN_NODE_BL_MAIN_MODULE)
    #include "can_library/generated/BL_MAIN_MODULE.h"
#elif defined(CAN_NODE_BL_REAR_DRIVELINE)
    #include "can_library/generated/BL_REAR_DRIVELINE.h"
#elif defined(CAN_NODE_BL_TORQUE_VECTOR)
    #include "can_library/generated/BL_TORQUE_VECTOR.h"
#elif defined(CAN_NODE_DASHBOARD)
    #include "can_library/generated/DASHBOARD.h"
#elif defined(CAN_NODE_DRIVELINE)
    #include "can_library/generated/DRIVELINE.h"
#elif defined(CAN_NODE_G4_TESTING)
    #include "can_library/generated/G4_TESTING.h"
#elif defined(CAN_NODE_MAIN_MODULE)
    #include "can_library/generated/MAIN_MODULE.h"
#elif defined(CAN_NODE_PDU)
    #include "can_library/generated/PDU.h"
#elif defined(CAN_NODE_TORQUE_VECTOR)
    #include "can_library/generated/TORQUE_VECTOR.h"
#endif

#endif // CAN_ROUTER_H
