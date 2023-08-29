#include <array>
#include <cstring>
#include <numeric>
#include <vector>

#include "../../src/core/DeviceHandler.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using namespace Finn;

TEST(DeviceHandlerTest, InitTest) {
    auto devicehandler = DeviceHandler("test", "someName", 0, {}, {});
    EXPECT_EQ(xrt::device::device_costum_constructor_called, 1);
    EXPECT_EQ(xrt::device::device_param_didx, 0);

    auto kernel_names = xrt::kernel::kernel_name;
    auto kernel_devices = xrt::kernel::kernel_device;
    auto kernel_uuids = xrt::kernel::kernel_uuid;

    EXPECT_TRUE(kernel_names.empty());
    EXPECT_TRUE(kernel_devices.empty());
    EXPECT_TRUE(kernel_uuids.empty());

    auto devicehandler2 = DeviceHandler("test", "someName", 4, {"inpName"}, {"outName"});
    EXPECT_EQ(xrt::device::device_costum_constructor_called, 2);

    kernel_names = xrt::kernel::kernel_name;
    kernel_devices = xrt::kernel::kernel_device;
    kernel_uuids = xrt::kernel::kernel_uuid;

    std::vector<std::string> ionames = {"inpName", "outName"};
    for (auto&& name : ionames) {
        if (std::find(kernel_names.begin(), kernel_names.end(), name) != kernel_names.end()) {
            continue;
        }
        FAIL();
    }

    const uuid_t id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    auto uuid = xrt::uuid(id);

    ASSERT_EQ(kernel_devices.size(), 2);

    EXPECT_EQ(kernel_devices[0].loadedUUID, kernel_devices[1].loadedUUID);
    // EXPECT_EQ(kernel_devices[0].loadedUUID, xrt::uuid(id));
    EXPECT_EQ(kernel_uuids[0], kernel_uuids[1]);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}