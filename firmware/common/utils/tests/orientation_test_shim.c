/**
 * @file orientation_test_shim.c
 * @brief Test orientation
 *
 * @author Kerem Eyce (keyce@purdue.edu)
 */


#include "orientation_test_shim.h"

mat3_t test_DCM_from_euler(const euler_angles_t angles) {
    return DCM_from_euler(angles);
}