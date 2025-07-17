/**
 * @file SyncInference.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Integrationtest for the Finn driver
 * @version 0.1
 * @date 2023-11-03
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/core/BaseDriver.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/join.hpp>
#include <numeric>

#include "gtest/gtest.h"

namespace Finn {
    template<bool SynchronousInference>
    using Driver = BaseDriver<SynchronousInference, DatatypeInt<8>, DatatypeUInt<16>>;
}

TEST(SyncInference, syncInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver<true>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1);

    Finn::vector<int8_t> data(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName), 1);

    std::iota(data.begin(), data.end(), -127);

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());

    Finn::vector<uint16_t> expectedResults = {98, 50, 65476, 65493, 27};

    EXPECT_EQ(results, expectedResults);
}

TEST(SyncInference, syncBatchInferenceTest) {
    std::string exampleNetworkConfig = "jetConfig.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    std::size_t batchLength = 10;

    auto driver = Finn::Driver<true>(conf, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, static_cast<uint>(batchLength));

    Finn::vector<int8_t> data(driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName) * batchLength, 1);

    for (std::size_t i = 0; i < batchLength; ++i) {
        std::iota(data.begin() + static_cast<decltype(data)::difference_type>(i * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)),
                  data.begin() + static_cast<decltype(data)::difference_type>((i + 1) * driver.getFeatureMapSize(0, conf.deviceWrappers[0].idmas[0]->kernelName)), -127 + static_cast<int>(i));
    }

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());

    Finn::vector<uint16_t> expectedResults = {98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27, 98, 50, 65476, 65493, 27,
                                              95, 61, 65483, 65491, 12, 98, 50, 65476, 65493, 27, 92, 53, 65483, 65498, 15, 92, 53, 65483, 65498, 15, 86, 53, 65489, 65498, 9};

    EXPECT_EQ(results, expectedResults);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::initLogger(true);

    return RUN_ALL_TESTS();
}