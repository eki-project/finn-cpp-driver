#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../src/core/DeviceHandler.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using namespace Finn;

class DeviceHandlerSetup : public ::testing::Test {
     protected:
    std::string fn = "somefile.xclbin";
    void SetUp() override {
        std::fstream tmpfile(fn, std::fstream::out);
        tmpfile << "some stuff\n";
        tmpfile.close();
    }

    // void TearDown() override { std::filesystem::remove(fn); }
};

/*

TEST_F(DeviceHandlerSetup, InitTest) {
    auto devicehandler = DeviceHandler("somefile.xclbin", "someName", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))});
    EXPECT_EQ(xrt::device::device_costum_constructor_called, 1);
    EXPECT_EQ(xrt::device::device_param_didx, 0);

    auto& kernel_names = xrt::kernel::kernel_name;
    auto& kernel_devices = xrt::kernel::kernel_device;
    auto& kernel_uuids = xrt::kernel::kernel_uuid;

    ASSERT_EQ(kernel_names.size(), 2);
    ASSERT_EQ(kernel_devices.size(), 2);
    ASSERT_EQ(kernel_uuids.size(), 2);

    kernel_names.clear();
    kernel_devices.clear();
    kernel_uuids.clear();

    auto devicehandler2 = DeviceHandler("somefile.xclbin", "someName", 4, {std::make_shared<BufferDescriptor>("inpName", shape_t({4}))}, {std::make_shared<BufferDescriptor>("outName", shape_t({4}))});
    EXPECT_EQ(xrt::device::device_costum_constructor_called, 2);

    std::vector<std::string> ionames = {"inpName", "outName"};
    for (auto&& name : ionames) {
        if (std::find(kernel_names.begin(), kernel_names.end(), name) != kernel_names.end()) {
            continue;
        }
        FAIL();
    }

    ASSERT_EQ(kernel_devices.size(), 2);

    EXPECT_EQ(kernel_devices[0].loadedUUID, kernel_devices[1].loadedUUID);
    EXPECT_EQ(kernel_uuids[0], kernel_uuids[1]);
}

TEST_F(DeviceHandlerSetup, ArgumentTest) {
    EXPECT_THROW(DeviceHandler("", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))}), std::filesystem::filesystem_error);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {std::make_shared<BufferDescriptor>("", shape_t({1}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({}))}), std::invalid_argument);
    EXPECT_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1}))}, {}), std::invalid_argument);
    EXPECT_NO_THROW(DeviceHandler("somefile.xclbin", "name", 0, {std::make_shared<BufferDescriptor>("a", shape_t({1})), std::make_shared<BufferDescriptor>("c", shape_t({1}))}, {std::make_shared<BufferDescriptor>("b", shape_t({1, 2}))}));
}
*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}