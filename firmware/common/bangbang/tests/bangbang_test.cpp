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

TEST_F(BangbangTest, AboveUpperBoundTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on) << "on_func did not call";
}

TEST_F(BangbangTest, BelowLowerBoundTurnsOff) {
    controller.is_on = true;
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND - 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on) << "off_func did not call";
}

TEST_F(BangbangTest, UpperBoundTurnsOn) {
    bangbang_update(&controller, UPPER_BOUND, MIN_SWITCH_INTERVAL + 1);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on) << "on_func did not call";
}

TEST_F(BangbangTest, LowerBoundTurnsOff) {
    controller.is_on = true;
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND, MIN_SWITCH_INTERVAL + 1);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on) << "off_func did not call";
}

TEST_F(BangbangTest, BetweenBoundsStaysOff) {
    bangbang_update(&controller, UPPER_BOUND - 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on);
}

TEST_F(BangbangTest, BetweenBoundsStaysOn) {
    controller.is_on = true;
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND + 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on);
}

TEST_F(BangbangTest, LessThanMinIntervalDoesNotSwitch) {
    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL - 1);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on);

    controller.is_on = true;
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND - 1, MIN_SWITCH_INTERVAL - 1);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on);
}

TEST_F(BangbangTest, LastSwitchMsUpdatesForSwitchOn) {
    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on);
    EXPECT_EQ(controller.last_switch_ms, MIN_SWITCH_INTERVAL + 1);

    bangbang_update(&controller, LOWER_BOUND - 1, MIN_SWITCH_INTERVAL + 2);
    EXPECT_TRUE(controller.is_on);
    EXPECT_TRUE(turned_on);
    EXPECT_EQ(controller.last_switch_ms, MIN_SWITCH_INTERVAL + 1) << "Second call updated last_switch_ms";
}

TEST_F(BangbangTest, LastSwitchMsUpdatesForSwitchOff) {
    controller.is_on = true;
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND - 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on);
    EXPECT_EQ(controller.last_switch_ms, MIN_SWITCH_INTERVAL + 1);

    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL + 2);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on);
    EXPECT_EQ(controller.last_switch_ms, MIN_SWITCH_INTERVAL + 1) << "Second call updated last_switch_ms";
}

TEST_F(BangbangTest, ExactIntervalPasses) {
    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL);
    EXPECT_TRUE(turned_on);
    EXPECT_TRUE(controller.is_on);

    bangbang_update(&controller, LOWER_BOUND - 1, 2 * MIN_SWITCH_INTERVAL);
    EXPECT_FALSE(controller.is_on);
    EXPECT_FALSE(turned_on);
}

TEST_F(BangbangTest, OnFunctionCallsWithoutStateSwitch) {
    controller.is_on = true;

    bangbang_update(&controller, UPPER_BOUND - 1, MIN_SWITCH_INTERVAL - 1);
    EXPECT_TRUE(turned_on) << "on_func not called";
}

TEST_F(BangbangTest, OffFunctionCallsWithoutStateSwitch) {
    turned_on = true;

    bangbang_update(&controller, LOWER_BOUND + 1, MIN_SWITCH_INTERVAL - 1);
    EXPECT_FALSE(turned_on) << "off_func not called";
}

TEST_F(BangbangTest, OnFunctionNullPointerDoesNotCall) {
    controller.on_func = nullptr;

    bangbang_update(&controller, UPPER_BOUND + 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_TRUE(controller.is_on) << "Controller state does not change";
}

TEST_F(BangbangTest, OffFunctionNullPointerDoesNotCall) {
    controller.off_func = nullptr;
    controller.is_on = true;

    bangbang_update(&controller, LOWER_BOUND - 1, MIN_SWITCH_INTERVAL + 1);
    EXPECT_FALSE(controller.is_on) << "Controller state does not change";
}

TEST_F(BangbangTest, LinearlyDecreasingInputFollowsHysteresis) {
    float value = 100.0f;
    uint32_t i;

    for (i = 0; i < 1000; i++) {
        value = 100.0f - (0.04f * i);
        bangbang_update(&controller, value, i);
        ASSERT_FALSE(controller.is_on) << "turned on too early, value is " << value \
                                                    << " and timer is " << i \
                                                    << " and last_switch_ms is " << controller.last_switch_ms;
        
        ASSERT_FALSE(turned_on) << "on_func was called when it was not supposed to";
    }

    for (; i < 2000; i++) {
        value = 100.0f - (0.04f * i);
        bangbang_update(&controller, value, i);
        ASSERT_TRUE(controller.is_on) << "turned off too early, value is " << value \
                                                   << ", timer is " << i \
                                                   << " and last_switch_ms is " << controller.last_switch_ms;

        ASSERT_TRUE(turned_on) << "off_func was called when it was not supposed to";
    }

    bangbang_update(&controller, value, i);
    EXPECT_FALSE(controller.is_on) << "failed last switch, value is " << value \
                                                << " and timer is " << i \
                                                << " and last_switch_ms is " << controller.last_switch_ms;
    
    EXPECT_FALSE(turned_on) << "on_func was called when it was not supposed to";
}