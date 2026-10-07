#include <gtest/gtest.h>

extern "C" {
#include "dearunits_test_shim.h"
}

namespace {

void check(float actual, float expected, float tolerance, const char *expression, int line) {
    EXPECT_NEAR(actual, expected, tolerance)
        << "dearunits_test_shim.c:" << line << ": " << expression;
}

} // namespace

TEST(DearUnitsTest, Basics) {
    dearunits_test_basics(check);
}

TEST(DearUnitsTest, Drivetrain) {
    dearunits_test_drivetrain(check);
}

TEST(DearUnitsTest, Battery) {
    dearunits_test_battery(check);
}

TEST(DearUnitsTest, Dynamics) {
    dearunits_test_dynamics(check);
}

TEST(DearUnitsTest, Brakes) {
    dearunits_test_brakes(check);
}

TEST(DearUnitsTest, SignalHelpers) {
    dearunits_test_signal_helpers(check);
}

