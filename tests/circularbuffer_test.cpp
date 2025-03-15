#include <gtest/gtest.h>
#include "../src/circularbuffer.h"
#include <string>

class CircularBufferTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试构造函数
TEST_F(CircularBufferTest, Constructor) {
    EXPECT_NO_THROW({
        CircularBuffer buffer(1024);
    });
}