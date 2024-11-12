#include "status.h"
#include "testing.h"
#include "gtest/gtest.h"

#include <thread>

class StatusTest : public ::testing::Test {};

TEST_F(StatusTest, testStatusConstructor) {
    auto status = alpaca::Status();

    EXPECT_OK(status);
}
