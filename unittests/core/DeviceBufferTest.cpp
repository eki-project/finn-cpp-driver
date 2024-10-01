/**
 * @file DeviceBufferTest.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Unittest for the Device Buffer
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/Logger.h>

#include <FINNCppDriver/core/DeviceBuffer/AsyncDeviceBuffers.hpp>
#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <memory>
#include <random>
#include <span>

#include "gtest/gtest.h"
#include "xrt/xrt_device.h"

// Provides config and shapes for testing
#include "UnittestConfig.h"
using namespace FinnUnittest;

class DBTest : public ::testing::Test {
     protected:
    xrt::device device;
    xrt::uuid uuid;
    FinnUtils::BufferFiller filler = FinnUtils::BufferFiller(0, 255);

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DBTest, DBStoreTest) {
    Finn::SyncDeviceInputBuffer<uint8_t> buffer("InputBuffer", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::vector<uint8_t> data(buffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE) * buffer.size(SIZE_SPECIFIER::BATCHSIZE));
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.store({data.begin(), data.end()});
    EXPECT_EQ(buffer.testGetMap(), data);
}

TEST_F(DBTest, DBOutputTest) {
    Finn::SyncDeviceOutputBuffer<uint8_t> buffer("OutputBuffer", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::vector<uint8_t> data(buffer.size(SIZE_SPECIFIER::TOTAL_DATA_SIZE));
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.testSetMap(data);
    buffer.read();
    auto vec = buffer.getData();
    EXPECT_EQ(data, vec);
}

TEST_F(DBTest, DBStoreTest_async) {
    Finn::AsyncDeviceInputBuffer<uint8_t> buffer("InputBuffer", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::vector<uint8_t> data(buffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE));
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.store({data.begin(), data.end()});
    sleep(2);  // Adjust timing as necessary
    EXPECT_EQ(buffer.testGetMap(), data);
}

TEST_F(DBTest, DBOutputTest_async) {
    Finn::AsyncDeviceOutputBuffer<uint8_t> buffer("OutputBuffer", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::vector<uint8_t> data(buffer.size(SIZE_SPECIFIER::TOTAL_DATA_SIZE)*2);
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.testSetMap(data);
    buffer.read();
    sleep(2);
    auto vec = buffer.getData();
    EXPECT_EQ(data, vec);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}