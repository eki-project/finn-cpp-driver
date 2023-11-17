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
#include <numeric>
#include <thread>

#include "gtest/gtest.h"

namespace Finn {
    using Driver = BaseDriver<DatatypeInt<8>, DatatypeUInt<16>>;
}

using namespace std::literals::chrono_literals;

TEST(SyncInference, syncInferenceTest) {
    std::string exampleNetworkConfig = "config.json";
    Finn::Config conf = Finn::createConfigFromPath(exampleNetworkConfig);

    auto driver = Finn::Driver(conf, 10, 0, conf.deviceWrappers[0].idmas[0]->kernelName, 0, conf.deviceWrappers[0].odmas[0]->kernelName, 1, true, false);

    Finn::vector<int8_t> data(driver.size(SIZE_SPECIFIER::ELEMENTS_PER_PART, 0, conf.deviceWrappers[0].idmas[0]->kernelName), 1);

    std::iota(data.begin(), data.end(), -127);

    // Run inference
    driver.input(data.begin(), data.end());
    std::this_thread::sleep_for(2000ms);
    auto results = driver.getResults();
    FINN_LOG(Logger::getLogger(), loglevel::info) << "Got results back from FPGA";
    std::cout << join(results, ",") << "\n";

    Finn::vector<uint16_t> expectedResults = {254, 510, 253, 509, 252};

    driver.~BaseDriver();

    EXPECT_EQ(results, expectedResults);

}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}