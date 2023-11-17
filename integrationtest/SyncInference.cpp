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
    using Driver = BaseDriver<DatatypeInt<8>, DatatypeUInt<16>>;
}

TEST(SyncInference, syncInferenceTest) {
    std::string exampleNetworkConfig = "config.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver(conf, 10, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1, true, true);

    Finn::vector<int8_t> data(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName), 1);

    std::iota(data.begin(), data.end(), -127);

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());

    Finn::vector<uint16_t> expectedResults = {254, 510, 253, 509, 252};

    EXPECT_EQ(results, expectedResults);
}

TEST(SyncInference, syncBatchInferenceTest) {
    std::string exampleNetworkConfig = "config.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);
    std::size_t batchLength = 10;

    auto driver = Finn::Driver(conf, static_cast<uint>(batchLength), 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, static_cast<uint>(batchLength), true, true);

    Finn::vector<int8_t> data(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName) * batchLength, 1);

    for (std::size_t i = 0; i < batchLength; ++i) {
        std::iota(data.begin() + static_cast<decltype(data)::difference_type>(i * driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName)),
                  data.begin() + static_cast<decltype(data)::difference_type>((i + 1) * driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName)), -127);
    }

    // Run inference
    auto results = driver.inferSynchronous(data.begin(), data.end());

    Finn::vector<uint16_t> expectedResults;

    for (std::size_t i = 0; i < batchLength; ++i) {
        expectedResults.insert(expectedResults.end(), {254, 510, 253, 509, 252});
    }

    EXPECT_EQ(results, expectedResults);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}