/**
 * @file BaseDriverTest.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Unittest for the base driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */


#include <FINNCppDriver/config/FinnDriverUsedDatatypes.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/core/BaseDriver.hpp>
#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/join.hpp>

#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

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

class TestDriver : public Finn::Driver<true> {
     public:
    TestDriver(const Finn::Config& pConfig, unsigned int hostBufferSize) : Finn::Driver<true>(pConfig, hostBufferSize) {}
    Finn::vector<uint8_t> inferR(const Finn::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex, const std::string& outputBufferKernelName, unsigned int samples,
                                 bool forceArchival) {
        return infer(data, inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
    }

    template<typename IterType>
    Finn::vector<uint8_t> inferR(IterType first, IterType last, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex, const std::string& outputBufferKernelName, unsigned int samples,
                                 bool forceArchival) {
        return infer(first, last, inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
    }
};

TEST_F(BaseDriverTest, BasicBaseDriverTest) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto driver = TestDriver(unittestConfig, hostBufferSize);

    Finn::vector<uint8_t> data;
    Finn::vector<uint8_t> backupData;
    data.resize(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, inputDmaName));

    filler.fillRandom(data.begin(), data.end());
    backupData = data;

    // Setup fake output data
    driver.getDeviceHandler(0).getOutputBuffer(outputDmaName)->testSetMap(data);

    // Run inference
    auto results = driver.inferR(data.begin(), data.begin() + 80, 0, inputDmaName, 0, outputDmaName, 1, 1);

    Finn::vector<uint8_t> base(data.begin(), data.begin() + static_cast<long int>(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, outputDmaName)));


    // Checks: That input and output data is the same is just for convenience, in application this does not need to be
    // Check output process
    EXPECT_EQ(results.size(), base.size());
    EXPECT_EQ(results, base);
    // Check input process
    auto testMap = driver.getDeviceHandler(0).getInputBuffer(inputDmaName).get()->testGetMap();
    EXPECT_TRUE(std::equal(testMap.begin(), testMap.begin() + 80, data.begin()));
}

TEST_F(BaseDriverTest, syncInferenceTest) {
    auto driver = Finn::Driver<true>(unittestConfig, hostBufferSize, 0, inputDmaName, 0, outputDmaName, 1, true);

    // The input has to be 4 times longer than the expected size of the FPGA, because uint8->int2 packing reduces size by factor 4
    std::cout << driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, inputDmaName) << "\n";
    Finn::vector<int8_t> data(300, 1);
    Finn::vector<uint8_t> outdata(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, outputDmaName), 1);

    // Setup fake output data
    driver.getDeviceHandler(0).getOutputBuffer(outputDmaName)->testSetMap(outdata);

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());

    Finn::vector<uint8_t> expected(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, outputDmaName), 1);

    EXPECT_EQ(results, expected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}