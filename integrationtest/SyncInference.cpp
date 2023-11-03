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

#include "gtest/gtest.h"

namespace Finn {
    using Driver = BaseDriver<DatatypeInt<8>, DatatypeUInt<16>>;
}

TEST(SyncInference, syncInferenceTest) {
    std::string exampleNetworkConfig = "../example_network/config.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver(conf, 10, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1, true);

    Finn::vector<int8_t> data(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName), 0);

    std::cout << "Input Size: " << data.size() << std::endl;

    // Run inference
    // auto results = driver.inferSynchronous(data.begin(), data.end());

    EXPECT_FALSE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}