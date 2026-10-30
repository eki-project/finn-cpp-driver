/**
 * @file AsyncDriverTest.cpp
 * @brief Unittests for the asynchronous driver path on the XRT mock
 *
 * The mock reports every kernel as idle immediately and never moves data, so the output worker keeps producing
 * zero-filled batches. That is enough to exercise the worker threads, the queues, callbacks, drain and the shutdown
 * ordering under the sanitizers.
 *
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 */

#include <FINNCppDriver/config/FinnDriverUsedDatatypes.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/core/BaseDriver.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/Logger.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "UnittestConfig.h"
#include "gtest/gtest.h"

using namespace FinnUnittest;
using namespace std::chrono_literals;

class AsyncDriverTest : public ::testing::Test {
     protected:
    // Own dummy bitstream name: the other core tests create and delete "finn-accel.xclbin" in the same directory, which
    // races when ctest runs the test binaries in parallel
    std::string fn = "finn-accel-async.xclbin";
    Finn::Config config = unittestConfig;
    void SetUp() override {
        std::fstream tmpfile(fn, std::fstream::out);
        tmpfile << "some stuff\n";
        tmpfile.close();
        for (auto& [index, wrapper] : config.deviceWrappers) {
            wrapper.xclbin = fn;
        }
    }
    void TearDown() override { std::filesystem::remove(fn); }
};

TEST_F(AsyncDriverTest, ConstructAndDestroyQuickly) {
    const auto start = std::chrono::steady_clock::now();
    {
        auto driver = Finn::Driver<false>(config, hostBufferSize);
        EXPECT_EQ(driver.getBatchSize(), hostBufferSize);
    }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

TEST_F(AsyncDriverTest, InputAndResultsRoundTrip) {
    auto driver = Finn::Driver<false>(config, hostBufferSize);
    const std::size_t inputElements = FinnUtils::shapeToElements(driver.getInputNormalShape(true));
    Finn::vector<int8_t> data(inputElements, 1);

    driver.input(data.begin(), data.end());
    auto results = driver.getResults();
    EXPECT_EQ(results.size(), FinnUtils::shapeToElements(driver.getOutputNormalShape(true)));

    // Wrong input size must be rejected before anything is queued
    Finn::vector<int8_t> tooShort(inputElements - 1, 1);
    EXPECT_THROW(driver.input(tooShort.begin(), tooShort.end()), std::runtime_error);
}

TEST_F(AsyncDriverTest, CallbackDrainAndShutdownWithPendingWork) {
    std::atomic<int> callbacks{0};
    const auto start = std::chrono::steady_clock::now();
    {
        auto driver = Finn::Driver<false>(config, hostBufferSize);
        driver.registerCallback(0, outputDmaName, [&callbacks](std::size_t) { ++callbacks; });
        const std::size_t inputElements = FinnUtils::shapeToElements(driver.getInputNormalShape(true));
        Finn::vector<int8_t> data(inputElements, 1);
        for (int i = 0; i < 3; ++i) {
            driver.input(data.begin(), data.end());
        }
        // The callback fires once the output worker has delivered a batch, without any consumer action; under the
        // sanitizers the first batch can take a while. getResults() blocks when nothing is queued, so it is only called
        // once a delivery is known to have happened
        for (int i = 0; i < 500 && callbacks.load() == 0; ++i) {
            std::this_thread::sleep_for(10ms);
        }
        ASSERT_GT(callbacks.load(), 0);
        EXPECT_EQ(driver.getResults().size(), FinnUtils::shapeToElements(driver.getOutputNormalShape(true)));
        driver.drain();
        // Re-registering while the worker is running must be safe
        driver.registerCallback(0, outputDmaName, [](std::size_t) {});
        driver.input(data.begin(), data.end());
    }
    EXPECT_GT(callbacks.load(), 0);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

TEST_F(AsyncDriverTest, SetBatchSizeRecreatesBuffers) {
    auto driver = Finn::Driver<false>(config, hostBufferSize);
    const auto start = std::chrono::steady_clock::now();
    driver.setBatchSize(hostBufferSize);  // same size: no work
    driver.setBatchSize(hostBufferSize * 2);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);  // the old implementation slept for 2 s
    EXPECT_EQ(driver.getBatchSize(), hostBufferSize * 2);
    EXPECT_EQ(driver.getBatchSize(0, inputDmaName), hostBufferSize * 2);

    const std::size_t inputElements = FinnUtils::shapeToElements(driver.getInputNormalShape(true));
    Finn::vector<int8_t> data(inputElements, 1);
    driver.input(data.begin(), data.end());
    EXPECT_EQ(driver.getResults().size(), FinnUtils::shapeToElements(driver.getOutputNormalShape(true)));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
