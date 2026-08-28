#include <gtest/gtest.h>

extern "C" {
    #include "strbuf.h"
}

#include <cstring>

constexpr size_t BUF_SIZE = 8;

// GoogleTest automatically calls 'SetUp()'
// data and sb get declared at construction (before Setup())
class StrbufTest : public ::testing::Test {
protected:
    void SetUp() override {
        sb.data = data;
        sb.length = 0;
        sb.max_len = BUF_SIZE;
    }

    uint8_t data[BUF_SIZE] = {};
    strbuf_t sb{};
};

TEST_F(StrbufTest, ClearResetsLength) {
    strbuf_append(&sb, "abc", 3);
    strbuf_clear(&sb);
    EXPECT_EQ(sb.length, 0);
}

TEST_F(StrbufTest, AppendWritesDataAndReturnsLength) {
    size_t written = strbuf_append(&sb, "abc", 3);
    EXPECT_EQ(written, 3U);
    EXPECT_EQ(sb.length, 3U);
    EXPECT_EQ(0, memcmp(sb.data, "abc", 3));
    EXPECT_EQ("abc", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, AppendAccumulatesAcrossCalls) {
    strbuf_append(&sb, "ab", 2);
    strbuf_append(&sb, "cd", 2);
    EXPECT_EQ(sb.length, 4U);
    EXPECT_EQ(0, memcmp(sb.data, "abcd", 4));
    EXPECT_EQ("abcd", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, AppendExactCapacitySucceeds) {
    size_t written = strbuf_append(&sb, "12345678", BUF_SIZE);
    EXPECT_EQ(written, BUF_SIZE);
    EXPECT_EQ(sb.length, BUF_SIZE);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", BUF_SIZE));
    EXPECT_EQ("12345678", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, AppendOverCapacityFailsAndLeavesBufferUnchanged) {
    size_t written = strbuf_append(&sb, "123456789", BUF_SIZE + 1);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 0U);
}

TEST_F(StrbufTest, AppendToFullBufferFails) {
    strbuf_append(&sb, "12345678", BUF_SIZE);
    size_t written = strbuf_append(&sb, "x", 1);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, BUF_SIZE);
}

TEST_F(StrbufTest, AppendZeroLengthReturnsZeroWithoutChangingState) {
    strbuf_append(&sb, "ab", 2);
    size_t written = strbuf_append(&sb, "x", 0);
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, 2U);
}

TEST_F(StrbufTest, RepeatedAppendCalls) {
    strbuf_append(&sb, "1", 1);
    strbuf_append(&sb, "2", 1);
    strbuf_append(&sb, "3", 1);
    strbuf_append(&sb, "4", 1);
    strbuf_append(&sb, "5", 1);
    strbuf_append(&sb, "6", 1);
    strbuf_append(&sb, "7", 1);
    strbuf_append(&sb, "8", 1);
    EXPECT_EQ(sb.length, 8U);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", 8));
    EXPECT_EQ("12345678", std::string(sb.data, sb.data + sb.length));
}


TEST_F(StrbufTest, RepeatedAppendCallsFails) {
    strbuf_append(&sb, "1", 1);
    strbuf_append(&sb, "2", 1);
    strbuf_append(&sb, "3", 1);
    strbuf_append(&sb, "4", 1);
    strbuf_append(&sb, "5", 1);
    strbuf_append(&sb, "6", 1);
    strbuf_append(&sb, "7", 1);
    strbuf_append(&sb, "8", 1);
    size_t written = strbuf_append(&sb, "9", 1);
    EXPECT_EQ(written, 0);
    EXPECT_EQ(sb.length, 8U);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", 8));
    EXPECT_EQ("12345678", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, PrintfWritesFormattedDataAndReturnsLength) {
    size_t written = strbuf_printf(&sb, "%d", 42);
    EXPECT_EQ(written, 2U);
    EXPECT_EQ(sb.length, 2U);
    EXPECT_EQ(0, memcmp(sb.data, "42", 2));
    EXPECT_EQ("42", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, PrintfAccumulatesAcrossCalls) {
    strbuf_printf(&sb, "%d", 1);
    strbuf_printf(&sb, "%d", 2);
    EXPECT_EQ(sb.length, 2U);
    EXPECT_EQ(0, memcmp(sb.data, "12", 2));
    EXPECT_EQ("12", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, RepeatedPrintfCalls) {
    strbuf_printf(&sb, "%d", 1);
    strbuf_printf(&sb, "%d", 2);
    strbuf_printf(&sb, "%d", 3);
    strbuf_printf(&sb, "%d", 4);
    strbuf_printf(&sb, "%d", 5);
    strbuf_printf(&sb, "%d", 6);
    strbuf_printf(&sb, "%d", 7);
    strbuf_printf(&sb, "%d", 8);
    EXPECT_EQ(sb.length, 8U);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", 8));
    EXPECT_EQ("12345678", std::string(sb.data, sb.data + sb.length));
}


TEST_F(StrbufTest, RepeatedPrintfCallsFails) {
    strbuf_printf(&sb, "%d", 1);
    strbuf_printf(&sb, "%d", 2);
    strbuf_printf(&sb, "%d", 3);
    strbuf_printf(&sb, "%d", 4);
    strbuf_printf(&sb, "%d", 5);
    strbuf_printf(&sb, "%d", 6);
    strbuf_printf(&sb, "%d", 7);
    strbuf_printf(&sb, "%d", 8);
    size_t written = strbuf_printf(&sb, "%d", 9);
    EXPECT_EQ(written, 0);
    EXPECT_EQ(sb.length, 8U);
    EXPECT_EQ(0, memcmp(sb.data, "12345678", 8));
    EXPECT_EQ("12345678", std::string(sb.data, sb.data + sb.length));
}

TEST_F(StrbufTest, PrintfOnFullBufferReturnsZeroImmediately) {
    strbuf_append(&sb, "12345678", BUF_SIZE);
    size_t written = strbuf_printf(&sb, "x");
    EXPECT_EQ(written, 0U);
    EXPECT_EQ(sb.length, BUF_SIZE);
}

TEST_F(StrbufTest, PrintfExceedingRemainingSpaceFailsAndLeavesBufferUnchanged) {
    // 7 bytes free; an 8-char input exceeds it.
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


TEST_F(StrbufTest, PrintfFillCapacity) {
    // With 7 bytes free, a 6-char result with null terminator
    // fills entire buffer
    strbuf_append(&sb, "x", 1);
    size_t written = strbuf_printf(&sb, "%s", "123456");
    EXPECT_EQ(written, 6U);
    EXPECT_EQ(sb.length, 7U);
}

TEST_F(StrbufTest, PrintfFillingAllButOneByteSucceeds) {
    // 7 bytes free; a 6-char string leaves a byte for the
    // terminator, which fits within max_len.
    strbuf_append(&sb, "x", 1);
    size_t written = strbuf_printf(&sb, "%s", "12345");
    EXPECT_EQ(written, 5U);
    EXPECT_EQ(sb.length, 6U);
    EXPECT_EQ(0, memcmp(sb.data + 1, "12345", 5));
}

TEST(StrbufAllocateMacro, InitializesFieldsCorrectly) {
    ALLOCATE_STRBUF(local, 16);
    EXPECT_EQ(local.length, 0U);
    EXPECT_EQ(local.max_len, 16U);
    ASSERT_NE(local.data, nullptr);

    strbuf_append(&local, "hi", 2);
    EXPECT_EQ(local.length, 2U);
    EXPECT_EQ(0, memcmp(local.data, "hi", 2));
    EXPECT_EQ("hi", std::string(local.data, local.data + local.length));
}
