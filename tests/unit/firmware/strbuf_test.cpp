#include <gtest/gtest.h>

extern "C" {
#include "strbuf.h"
#include "strbuf_test_shim.h"
}

#include <cstring>

namespace {

constexpr size_t kBufSize = 8;

class StrbufTest : public ::testing::Test {
protected:
    void SetUp() override {
        sb.data = data;
        sb.length = 0;
        sb.max_len = kBufSize;
    }

    uint8_t data[kBufSize] = {};
    strbuf_t sb{};
};

}  // namespace

TEST_F(StrbufTest, ClearResetsLength) {
    strbuf_append(&sb, "abc", 3);
    strbuf_clear(&sb);
    EXPECT_EQ(sb.length, 0U);
}

TEST_F(StrbufTest, AppendWritesDataAndReturnsLength) {
    size_t written = strbuf_append(&sb, "abc", 3);
    EXPECT_EQ(written, 3U);
    EXPECT_EQ(sb.length, 3U);
    EXPECT_EQ(0, memcmp(sb.data, "abc", 3));
}

TEST_F(StrbufTest, AppendAccumulatesAcrossCalls) {
    strbuf_append(&sb, "ab", 2);
    strbuf_append(&sb, "cd", 2);
    EXPECT_EQ(sb.length, 4U);
    EXPECT_EQ(0, memcmp(sb.data, "abcd", 4));
}

TEST_F(StrbufTest, AppendExactCapacitySucceeds) {
    size_t written = strbuf_append(&sb, "12345678", kBufSize);
    EXPECT_EQ(written, kBufSize);
    EXPECT_EQ(sb.length, kBufSize);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", kBufSize));
}

TEST_F(StrbufTest, AppendOverCapacityFailsAndLeavesBufferUnchanged) {
    size_t written = strbuf_append(&sb, "123456789", kBufSize + 1);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 0U);
}

TEST_F(StrbufTest, AppendToFullBufferFails) {
    strbuf_append(&sb, "12345678", kBufSize);
    size_t written = strbuf_append(&sb, "x", 1);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, kBufSize);
}

TEST_F(StrbufTest, AppendZeroLengthReturnsZeroWithoutChangingState) {
    strbuf_append(&sb, "ab", 2);
    size_t written = strbuf_append(&sb, "x", 0);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 2U);
}

TEST_F(StrbufTest, PrintfWritesFormattedDataAndReturnsLength) {
    size_t written = strbuf_printf(&sb, "%d", 42);
    EXPECT_EQ(written, 2U);
    EXPECT_EQ(sb.length, 2U);
    EXPECT_EQ(0, memcmp(sb.data, "42", 2));
}

TEST_F(StrbufTest, PrintfAccumulatesAcrossCalls) {
    strbuf_printf(&sb, "%d", 1);
    strbuf_printf(&sb, "%d", 2);
    EXPECT_EQ(sb.length, 2U);
    EXPECT_EQ(0, memcmp(sb.data, "12", 2));
}

TEST_F(StrbufTest, PrintfOnFullBufferReturnsZeroImmediately) {
    strbuf_append(&sb, "12345678", kBufSize);
    size_t written = strbuf_printf(&sb, "x");
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, kBufSize);
}

TEST_F(StrbufTest, PrintfExceedingRemainingSpaceFailsAndLeavesBufferUnchanged) {
    // 7 bytes free; an 8-char result clearly exceeds it.
    strbuf_append(&sb, "x", 1);
    size_t written = strbuf_printf(&sb, "%s", "12345678");
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 1U);
}

TEST_F(StrbufTest, PrintfExactlyFillingRemainingSpaceFails) {
    // With 7 bytes free, a 7-char result leaves no room for the
    // terminator vsnprintf writes internally, so it must be rejected.
    strbuf_append(&sb, "x", 1);
    size_t written = strbuf_printf(&sb, "%s", "1234567");
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 1U);
}

TEST_F(StrbufTest, PrintfFillingAllButOneByteSucceeds) {
    // 7 bytes free; a 6-char result leaves exactly 1 byte for the
    // terminator, which fits within max_len.
    strbuf_append(&sb, "x", 1);
    size_t written = strbuf_printf(&sb, "%s", "12345");
    EXPECT_EQ(written, 5U);
    EXPECT_EQ(sb.length, 6U);
    EXPECT_EQ(0, memcmp(sb.data + 1, "12345", 5));
}

TEST(StrbufAllocateMacro, InitializesFieldsCorrectly) {
    uint8_t out_data[16] = {};
    allocate_strbuf_result_t result = test_allocate_strbuf_and_append("hi", 2, out_data, sizeof(out_data));

    EXPECT_EQ(result.max_len, 16U);
    EXPECT_FALSE(result.data_ptr_is_null);
    EXPECT_EQ(result.length, 2U);
    EXPECT_EQ(0, memcmp(out_data, "hi", 2));
}
