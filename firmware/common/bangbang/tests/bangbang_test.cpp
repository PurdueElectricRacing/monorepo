/**
 * @file bangbang_test.cpp
 * @brief Bang-bang controller update function unit tests
 * 
 * @author Kyle Yang (yang3122@purdue.edu)
 */

#include <gtest/gtest.h>

extern "C" {
#include "bangbang.h"
}

namespace {

constexpr float UPPER_BOUND = 50.0f;
constexpr float LOWER_BOUND = 25.0f;

bool turnedOn;

void onCallback() {
    turnedOn = true;
}

void offCallback() {
    turnedOn = false;
}

} // namespace
class BangbangTest : public ::testing::Test {
  protected:
    void SetUp() override {
        controller.upper_bound = UPPER_BOUND;
        controller.lower_bound = LOWER_BOUND;
        controller.on_func = onCallback;
        controller.off_func = offCallback;
        controller.last_switch_ms = 0;
        controller.min_switch_interval = 1000;
        controller.is_on = false;
        turnedOn = false;
    }

    bangbang_t controller;
};

// Test that last saved ms works

// Test upper bounds
TEST_F(BangbangTest, AboveUpperBoundTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND+1, 2000);
    EXPECT_EQ(turnedOn, true);

    // Turn the controller off

    // Set time_since_last_switch to be above the min_switch_interval

    // Pass a value above UPPER_BOUND using bangbang_update()

    // Check if the controller turns on
}

// Test lower bounds

// Test that min interval works (quick switching)

// Test edge cases for upper and lower bounds and switch interval

// Test moving value (y = mx + b) for hysteresis

// Test null pointers for functions