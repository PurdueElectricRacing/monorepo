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

bool turned_on;

void onCallback() {
    turned_on = true;
}

void offCallback() {
    turned_on = false;
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
        turned_on = false;
    }

    bangbang_t controller;
};

// TODO: Add messages for test cases for easier debugging
// TODO: Check if tests are simplified to exactly what is to be tested

// Test upper bounds
TEST_F(BangbangTest, AboveUpperBoundTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(turned_on, true);
    EXPECT_EQ(controller.is_on, true);
}

// Test lower bounds
TEST_F(BangbangTest, BelowLowerBoundTurnsOff) {
    controller.is_on = true;
    turned_on = true;
    ASSERT_EQ(turned_on, true);
    ASSERT_EQ(controller.is_on, true);

    bangbang_update(&controller, LOWER_BOUND-1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(controller.is_on, false);
    EXPECT_EQ(turned_on, false);
}

// Test that min interval works
TEST_F(BangbangTest, LessThanMinIntervalDoesNotSwitch) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL-1);
    EXPECT_EQ(controller.is_on, false);
    EXPECT_EQ(turned_on, false);

    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(controller.is_on, true);
    EXPECT_EQ(turned_on, true);
    
    controller.last_switch_ms = 0;

    bangbang_update(&controller, LOWER_BOUND-1, MIN_SWITCH_INTERVAL-1);
    EXPECT_EQ(controller.is_on, true);
    EXPECT_EQ(turned_on, true) << "controller called offCallback";
    // When I pass MIN_SWITCH_INTERVAL+1 and a valid upper bounds, do not reset last_switch_ms,
    // then pass MIN_SWITCH_INTERVAL-1 and a valid lower bounds, the controller will switch.
    // As time_since_last_int is an unsigned int, unsigned arithmetic overflow will wrap to 
    // a large positive number. Unknown if this can be an issue.
}

// Test edge cases for upper and lower bounds and switch interval
TEST_F(BangbangTest, UpperBoundsTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(turned_on, true);
    EXPECT_EQ(controller.is_on, true);
}

TEST_F(BangbangTest, LowerBoundTurnsOff) {
    controller.is_on = true;
    turned_on = true;
    ASSERT_EQ(turned_on, true);
    ASSERT_EQ(controller.is_on, true);

    bangbang_update(&controller, LOWER_BOUND, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(controller.is_on, false);
    EXPECT_EQ(turned_on, false);
}

TEST_F(BangbangTest, ExactIntervalPasses) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL);
    EXPECT_EQ(turned_on, true);
    EXPECT_EQ(controller.is_on, true);

    bangbang_update(&controller, LOWER_BOUND-1, 2*MIN_SWITCH_INTERVAL);
    EXPECT_EQ(controller.is_on, false);
    EXPECT_EQ(turned_on, false);
}

// Test last_saved_ms
TEST_F(BangbangTest, LastSavedMsIsRetained) {
    bangbang_update(&controller, UPPER_BOUND+1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(turned_on, true);
    EXPECT_EQ(controller.is_on, true);
    EXPECT_EQ(controller.last_switch_ms, MIN_SWITCH_INTERVAL+1);

    bangbang_update(&controller, LOWER_BOUND-1, MIN_SWITCH_INTERVAL+1);
    EXPECT_EQ(controller.is_on, true);
    EXPECT_EQ(turned_on, true);
}

// Test moving value (y = mx + b)

// Test null pointers for functions
TEST_F(BangbangTest, OnFunctionNullPointerDoesNotCall) {
    // Give BangbangTest nullptr for on_func

    // Try to call on_func using bangbang_update,
    // should not run anything but is_on is true
    // So should complete normally
}

TEST_F(BangbangTest, OffCallbackNullPointerDoesNotCall) {
    // Give BangbangTest nullptr for off_func

    // Set is_on and turned_on to true

    // Try to call off_func using bangbang_update,
    // should not run anything but is_on is false
    // So should complete normally
}