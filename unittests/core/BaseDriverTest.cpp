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
#include <FINNCppDriver/core/DeviceBuffer.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>

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

class TestDriver : public Finn::Driver {
     public:
    TestDriver(const Finn::Config& pConfig, unsigned int hostBufferSize) : Finn::Driver(pConfig, hostBufferSize) {}
    Finn::vector<uint8_t> inferR(const Finn::vector<uint8_t>& data, unsigned int inputDeviceIndex, const std::string& inputBufferKernelName, unsigned int outputDeviceIndex, const std::string& outputBufferKernelName, unsigned int samples,
                                 bool forceArchival) {
        return inferRaw(data, inputDeviceIndex, inputBufferKernelName, outputDeviceIndex, outputBufferKernelName, samples, forceArchival);
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
    driver.getDeviceHandler(0).getOutputBuffer(outputDmaName).testSetMap(data);

    // Run inference
    auto results = driver.inferR(data, 0, inputDmaName, 0, outputDmaName, 1, 1);


    // Checks: That input and output data is the same is just for convenience, in application this does not need to be
    // Check output process
    EXPECT_EQ(results, data);
    // Check input process
    EXPECT_EQ(driver.getDeviceHandler(0).getInputBuffer(inputDmaName).testGetMap(), data);
}

TEST_F(BaseDriverTest, syncInferenceTest) {
    auto filler = FinnUtils::BufferFiller(0, 255);
    auto driver = Finn::Driver(unittestConfig, hostBufferSize, 0, inputDmaName, 0, outputDmaName, 1, 1);

    Finn::vector<uint8_t> data;
    Finn::vector<uint8_t> backupData;
    data.resize(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, inputDmaName));

    filler.fillRandom(data.begin(), data.end());
    backupData = data;

    // Setup fake output data
    driver.getDeviceHandler(0).getOutputBuffer(outputDmaName).testSetMap(data);

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());


    // // Checks: That input and output data is the same is just for convenience, in application this does not need to be
    // // Check output process
    // EXPECT_EQ(results[0], data);
    // // Check input process
    // EXPECT_EQ(driver.getDeviceHandler(0).getInputBuffer(inputDmaName).testGetMap(), data);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}