#include <gtest/gtest.h>

extern "C" {
#include "linear_algebra.h"
}

static void expect_vector(vec3_t actual, vec3_t expected) {
    EXPECT_NEAR(actual.x, expected.x, 1e-6f);
    EXPECT_NEAR(actual.y, expected.y, 1e-6f);
    EXPECT_NEAR(actual.z, expected.z, 1e-6f);
}

TEST(Vector3, Arithmetic) {
    const vec3_t a = {1.0f, -2.0f, 3.0f};
    const vec3_t b = {4.0f, 5.0f, -6.0f};

    expect_vector(vec3_add(a, b), {5.0f, 3.0f, -3.0f});
    expect_vector(vec3_sub(a, b), {-3.0f, -7.0f, 9.0f});
    expect_vector(vec3_scale(a, -2.0f), {-2.0f, 4.0f, -6.0f});
}

TEST(Vector3, MagnitudeAndNormalization) {
    const vec3_t v = {3.0f, 4.0f, 0.0f};

    EXPECT_FLOAT_EQ(vec3_mag(v), 5.0f);
    expect_vector(vec3_normalize(v), {0.6f, 0.8f, 0.0f});

    expect_vector(vec3_normalize({0.0f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f});
    expect_vector(vec3_normalize({0.00001f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f});
}
