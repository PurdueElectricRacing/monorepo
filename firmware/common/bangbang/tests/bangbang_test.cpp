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

namespace bangbang_testing {

constexpr float UPPER_BOUND = 50.0f;
constexpr float LOWER_BOUND = 25.0f;

void onCallback() {

}

void offCallback() {

}

} // namespace bangbang_testing
class BangbangTest: public testing::Test {
  protected:
    void SetUp() override {
        controller.upper_bound = bangbang_testing::UPPER_BOUND;
        controller.lower_bound = bangbang_testing::LOWER_BOUND;
        controller.on_func = bangbang_testing::onCallback;
        controller.off_func = bangbang_testing::offCallback;
        controller.last_switch_ms = 0;
        controller.min_switch_interval = 1000;
        controller.is_on = false;
    }

    bangbang_t controller;
};

// Test that last saved ms works

// Test upper bounds
TEST_F(BangbangTest, AboveUpperBoundTurnsOn) {
    // Turn the controller off

    // Set time_since_last_switch to be above the min_switch_interval

    // Pass a value above UPPER_BOUND using bangbang_update()

    // Check if the controller turns on
}

// Test lower bounds

// Test that min interval works (quick switching)

// Test edge cases for upper and lower bounds

// Test null pointers for functions