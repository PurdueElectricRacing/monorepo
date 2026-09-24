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
constexpr uint32_t MIN_SWITCH_INTERVAL = 1000;

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
        controller.min_switch_interval = MIN_SWITCH_INTERVAL;
        controller.is_on = false;
        turnedOn = false;
    }

    bangbang_t controller;
};

// Test that last saved ms works

// Test upper bounds
TEST_F(BangbangTest, AboveUpperBoundTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(turnedOn, true);
    EXPECT_EQ(controller.is_on, true);
}

// Test lower bounds
TEST_F(BangbangTest, BelowLowerBoundTurnsOff) {
    controller.is_on = true;
    turnedOn = true;
    ASSERT_EQ(turnedOn, true);
    ASSERT_EQ(controller.is_on, true);

    bangbang_update(&controller, LOWER_BOUND-1, MIN_SWITCH_INTERVAL);
    EXPECT_EQ(controller.is_on, false);
    EXPECT_EQ(turnedOn, false);
}

// Test that min interval works
TEST_F(BangbangTest, LessThanMinIntervalDoesNotSwitch) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL-1);
    ASSERT_EQ(controller.is_on, false);
    ASSERT_EQ(turnedOn, false);

    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL+1);
    ASSERT_EQ(controller.is_on, true);
    ASSERT_EQ(turnedOn, true);
    
    controller.last_switch_ms = 0;

    bangbang_update(&controller, LOWER_BOUND-1, MIN_SWITCH_INTERVAL-1);
    EXPECT_EQ(controller.is_on, true);
    EXPECT_EQ(turnedOn, true) << "controller called offCallback";
    // When I pass MIN_SWITCH_INTERVAL+1 and a valid upper bounds, do not reset last_switch_ms,
    // then pass MIN_SWITCH_INTERVAL-1 and a valid lower bounds, the controller will switch.
    // As time_since_last_int is an unsigned int, unsigned arithmetic overflow will wrap to 
    // a large positive number. Unknown if this can be an issue.
}

// Test edge cases for upper and lower bounds and switch interval

// Test moving value (y = mx + b)

// Test null pointers for functions