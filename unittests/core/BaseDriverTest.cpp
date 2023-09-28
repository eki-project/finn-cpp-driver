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

class BaseDriverTest : public ::testing::Test {
     protected:
    std::string fn = "finn-accel.xclbin";
    void SetUp() override {
        std::fstream tmpfile(fn, std::fstream::out);
        tmpfile << "some stuff\n";
        tmpfile.close();
    }

    void TearDown() override { std::filesystem::remove(fn); }
};

TEST_F(BaseDriverTest, BasicBaseDriverTest) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto driver = Finn::Driver(unittestConfig, hostBufferSize);

    std::vector<uint8_t> data;
    std::vector<uint8_t> backupData;
    data.resize(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, inputDmaName));

    filler.fillRandom(data);
    backupData = data;

    // Setup fake output data
    driver.getDeviceHandler(0).getOutputBuffer(outputDmaName).testSetMap(data);

    // Run inference
    auto results = driver.inferRaw(data, 0, inputDmaName, 0, outputDmaName, 1, 1);


    // Checks: That input and output data is the same is just for convenience, in application this does not need to be
    // Check output process
    EXPECT_EQ(results[0], data);
    // Check input process
    EXPECT_EQ(driver.getDeviceHandler(0).getInputBuffer(inputDmaName).testGetMap(), data); 


}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}