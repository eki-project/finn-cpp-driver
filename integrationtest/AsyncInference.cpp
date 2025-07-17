/**
 * @file SyncInference.cpp
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
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <thread>

#include "gtest/gtest.h"

namespace Finn {
    template<bool SynchronousInference>
    using Driver = BaseDriver<SynchronousInference, DatatypeInt<8>, DatatypeUInt<16>>;
}

using namespace std::literals::chrono_literals;

TEST(AsyncInference, asyncInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1);

    Finn::vector<int8_t> data(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName), 1);

    std::iota(data.begin(), data.end(), -127);

    // Run inference
    driver.input(data.begin(), data.end());
    auto results = driver.getResults();  // This should block until the results are available

    Finn::vector<uint16_t> expectedResults = {98, 50, 65476, 65493, 27};

    EXPECT_EQ(results, expectedResults);
}

TEST(AsyncInference, asyncBatchInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    std::size_t batchLength = 10;
    std::atomic<std::size_t> availableData(0);
    std::condition_variable cv;
    std::mutex m;


    // BUG HIER IRGENDWO SODASS FEATUREMAPSIZE UND TOTALDATASIZE GLEICH SIND
    auto driver = Finn::Driver<false>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, static_cast<uint>(batchLength));

    Finn::vector<int8_t> data(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName) * batchLength, 1);

    for (std::size_t i = 0; i < batchLength; ++i) {
        std::iota(data.begin() + static_cast<decltype(data)::difference_type>(i * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)),
                  data.begin() + static_cast<decltype(data)::difference_type>((i + 1) * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)), -127 + static_cast<int>(i));
    }

    // Run inference
    driver.input(data.begin(), data.end());
    auto results = driver.getResults();  // This should block until the results are available

    Finn::vector<uint16_t> expectedResults = {98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27,
                                              95, 61, 65483, 65491, 12, 98, 50, 65476, 65493, 27, 92, 53, 65483, 65498, 15, 92, 53, 65483, 65498, 15, 86, 53, 65489, 65498, 9};

    EXPECT_EQ(results.size(), expectedResults.size());

    EXPECT_EQ(results, expectedResults);

    for (std::size_t i = 0; i < batchLength; ++i) {
        std::iota(data.begin() + static_cast<decltype(data)::difference_type>(i * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)),
                  data.begin() + static_cast<decltype(data)::difference_type>((i + 1) * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)), -127 + static_cast<int>(i));
    }

    driver.registerCallback(0, conf.deviceWrappers[0].odmas[0]->kernelName, [&availableData, &cv](std::size_t numItems) {
        availableData += numItems;
        cv.notify_all();
    });

    // Run inference
    driver.input(data.begin(), data.end());
    results.clear();

    // wait until output thread notifies
    std::unique_lock lk(m);
    cv.wait(lk, [&availableData, &driver, &conf] { return availableData >= driver.getTotalDataSize(0, conf.deviceWrappers[0].odmas[0]->kernelName); });

    results = driver.getResults();

    EXPECT_EQ(results.size(), expectedResults.size());

    EXPECT_EQ(results, expectedResults);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::initLogger(true);

    return RUN_ALL_TESTS();
}