#include "../../src/core/DeviceBuffer.hpp"
#include "../../src/core/BaseDriver.hpp"
#include "../../src/utils/FinnDatatypes.hpp"
#include "../../src/utils/Logger.h"
#include "../../src/utils/Types.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "../../src/utils/FinnUtils.h"
#include "../../src/config/FinnDriverUsedDatatypes.h"

// Provides config and shapes
#include "UnittestConfig.h"
using namespace FinnUnittest;

TEST(BaseDriverTest, BasicBaseDriverTest) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto driver = Finn::Driver(unittestConfig, hostBufferSize);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}