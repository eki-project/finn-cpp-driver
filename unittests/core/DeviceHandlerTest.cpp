#include "../../src/core/DeviceHandler.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"

using namespace Finn;

TEST(DeviceHandlerTest, Test) {
    DeviceHandler("test", "someName", 0, {}, {});
    xrt::device(5);
    EXPECT_EQ(1, 1);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}