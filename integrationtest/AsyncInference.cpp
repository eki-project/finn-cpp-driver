/**
 * @file AsyncInference.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Integrationtest for the Finn driver
 * @version 0.1
 * @date 2023-11-16
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */


#include <FINNCppDriver/core/BaseDriver.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/join.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <thread>

#include "gtest/gtest.h"

namespace Finn {
    // The jet example network is INT8 -> INT16 (see example_networks/*/FinnDatatypesJet.h)
    template<bool SynchronousInference>
    using Driver = BaseDriver<SynchronousInference, DatatypeInt<8>, DatatypeInt<16>>;
}

using namespace std::literals::chrono_literals;

namespace {
    const Finn::vector<int16_t> expectedSingle = {98, 50, -60, -43, 27};
    const Finn::vector<int16_t> expectedBatch = {98, 50, -60, -43, 27, 98, 50, -60, -43, 27, 98, 50, -60, -43, 27, 98, 50, -60, -43, 27, 98, 50, -60, -43, 27,
                                                 95, 61, -53, -45, 12, 98, 50, -60, -43, 27, 92, 53, -53, -38, 15, 92, 53, -53, -38, 15, 86, 53, -47, -38, 9};

    Finn::vector<int8_t> makeBatchInput(std::size_t featureMapSize, std::size_t batchLength) {
        Finn::vector<int8_t> data(featureMapSize * batchLength, 1);
        for (std::size_t i = 0; i < batchLength; ++i) {
            std::iota(data.begin() + static_cast<decltype(data)::difference_type>(i * featureMapSize), data.begin() + static_cast<decltype(data)::difference_type>((i + 1) * featureMapSize), static_cast<int8_t>(-127 + static_cast<int>(i)));
        }
        return data;
    }
}  // namespace

TEST(AsyncInference, asyncInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1);

    Finn::vector<int8_t> data(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName), 1);

    std::iota(data.begin(), data.end(), -127);

    // Run inference
    driver.input(data.begin(), data.end());
    auto results = driver.getResults();  // Blocks until the results are available

    EXPECT_EQ(results, expectedSingle);
}

TEST(AsyncInference, asyncBatchInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    const std::size_t batchLength = 10;
    std::size_t availableData = 0;
    std::condition_variable cv;
    std::mutex m;

    auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, static_cast<uint>(batchLength));

    const std::size_t featureMapSize = driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName);
    auto data = makeBatchInput(featureMapSize, batchLength);

    // Run inference
    driver.input(data.begin(), data.end());
    auto results = driver.getResults();  // Blocks until the results are available

    EXPECT_EQ(results.size(), expectedBatch.size());
    EXPECT_EQ(results, expectedBatch);

    // Second batch, this time waiting for the callback before fetching the results
    driver.registerCallback(0, conf.deviceWrappers[0].odmas[0]->kernelName, [&availableData, &cv, &m](std::size_t numItems) {
        {
            // The callback reports how many complete batches worth of elements are queued
            const std::lock_guard<std::mutex> lock(m);  // notify under the mutex so the waiter cannot miss it
            availableData = numItems;
        }
        cv.notify_all();
    });

    driver.input(data.begin(), data.end());
    results.clear();

    {
        std::unique_lock lk(m);
        const bool notified = cv.wait_for(lk, 30s, [&availableData, &driver, &conf] { return availableData >= driver.getTotalDataSize(0, conf.deviceWrappers[0].odmas[0]->kernelName); });
        ASSERT_TRUE(notified) << "Output callback was not called within 30 s";
    }

    results = driver.getResults();

    EXPECT_EQ(results.size(), expectedBatch.size());
    EXPECT_EQ(results, expectedBatch);
}

// Several batches in flight: the driver must pipeline them and keep the order
TEST(AsyncInference, asyncPipelinedInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    const std::size_t batchLength = 10;
    const int batches = 50;

    auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, static_cast<uint>(batchLength));
    const std::size_t featureMapSize = driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName);
    auto data = makeBatchInput(featureMapSize, batchLength);

    std::thread producer([&] {
        for (int i = 0; i < batches; ++i) {
            driver.input(data.begin(), data.end());
        }
    });
    int matching = 0;
    for (int i = 0; i < batches; ++i) {
        const auto results = driver.getResults();
        if (results == expectedBatch) {
            ++matching;
        } else {
            std::string got;
            for (std::size_t k = 0; k < results.size(); ++k) {
                got += (k > 0 ? " " : "") + std::to_string(results[k]);
            }
            ADD_FAILURE() << "batch " << i << " of " << batches << " differs (" << results.size() << " values): " << got;
        }
    }
    producer.join();
    EXPECT_EQ(matching, batches);
}

// Destroying a driver with a batch still queued must not hang or crash
TEST(AsyncInference, shutdownWithPendingInput) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    const auto start = std::chrono::steady_clock::now();
    {
        auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 10);
        auto data = makeBatchInput(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName), 10);
        driver.input(data.begin(), data.end());
        std::this_thread::sleep_for(100ms);
    }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 30s);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::initLogger(true);

    return RUN_ALL_TESTS();
}
