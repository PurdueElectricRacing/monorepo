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

namespace TestCallbacks {
    void onCallback() {

    }

    void offCallback() {

    }
}
class BangbangTest: public testing::Test {
  protected:
    void SetUp() override {
        controller.upper_bound = 50.0f;
        controller.lower_bound = 25.0f;
        controller.on_func = TestCallbacks::onCallback;
        controller.off_func = TestCallbacks::offCallback;
        controller.last_switch_ms = 0;
        controller.min_switch_interval = 1000;
        controller.is_on = false;
    }

    bangbang_t controller;
};

// Create a bang-bang controller object

// Test that last saved ms works

// Test upper bounds

// Test lower bounds

// Test that min interval works (quick switching)

// Test edge cases for upper and lower bounds

// Test null pointers for functions