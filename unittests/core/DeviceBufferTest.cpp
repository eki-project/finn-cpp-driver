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

#include <FINNCppDriver/core/DeviceBuffer/AsyncDeviceBuffers.hpp>
#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/Logger.hpp>
#include <chrono>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <thread>

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
    Finn::vector<uint8_t> data(buffer.getFeatureMapSize() * buffer.getBatchSize());
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.store({data.begin(), data.end()});
    EXPECT_EQ(buffer.testGetMap(), data);
}

TEST_F(DBTest, DBOutputTest) {
    Finn::SyncDeviceOutputBuffer<uint8_t> buffer("OutputBuffer", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::vector<uint8_t> data(buffer.getTotalDataSize());
    FinnUtils::BufferFiller(0, 255).fillRandom(data.begin(), data.end());
    buffer.testSetMap(data);
    buffer.read();
    auto vec = buffer.getData(buffer.getTotalDataSize());
    EXPECT_EQ(data, vec);
}

TEST_F(DBTest, RingPostsDescriptors) {
    // ring DMA with 4 slots; the mock completes every posted descriptor immediately
    const Finn::RingControl ring{4, 0x20, 0x30, 0x40, 0x50, 0x58};
    xrt::ip::ringCompletesImmediately = std::make_pair(ring.prod, ring.cons);
    Finn::SyncDeviceInputBuffer<uint8_t> buffer("RingInput", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts, ring);
    auto reg = [](uint32_t offset) { return xrt::ip::registers[{"RingInput", offset}]; };
    for (uint32_t posted = 1; posted <= 10; ++posted) {  // more descriptors than slots
        EXPECT_TRUE(buffer.run());
        EXPECT_TRUE(buffer.wait());
        EXPECT_EQ(reg(ring.prod), posted);
        const uint32_t slot = 4 * ((posted - 1) % ring.slots);
        EXPECT_EQ(reg(ring.frames + slot), FinnUnittest::parts);
        EXPECT_EQ(reg(ring.addrLo + slot) | (static_cast<uint64_t>(reg(ring.addrHi + slot)) << 32), static_cast<uint64_t>(buffer.testGetAddress()));
    }
    xrt::ip::ringCompletesImmediately.reset();
}

TEST_F(DBTest, RingContinuesKernelCount) {
    // the kernel never restarts: a new buffer continues from the count it finds
    const Finn::RingControl ring{4, 0x20, 0x30, 0x40, 0x50, 0x58};
    xrt::ip::ringCompletesImmediately = std::make_pair(ring.prod, ring.cons);
    xrt::ip::registers[{"RingOutput", ring.prod}] = 7;
    Finn::SyncDeviceOutputBuffer<uint8_t> buffer("RingOutput", device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts, ring);
    EXPECT_TRUE(buffer.run());
    EXPECT_TRUE(buffer.wait());
    EXPECT_EQ((xrt::ip::registers[{"RingOutput", ring.prod}]), 8u);
    EXPECT_EQ((xrt::ip::registers[{"RingOutput", ring.frames + 4 * (7 % ring.slots)}]), FinnUnittest::parts);
    xrt::ip::ringCompletesImmediately.reset();
}

TEST_F(DBTest, RingAsyncOutputKeepsNextDescriptorPosted) {
    // the output worker posts the next announced batch while the kernel is still writing the current one, and collects
    // the batches in order as the kernel completes them
    using namespace std::chrono_literals;
    const Finn::RingControl ring{4, 0x20, 0x30, 0x40, 0x50, 0x58};
    const std::string name = "RingAsyncOutput";
    auto gate = std::make_shared<Finn::detail::BatchGate>(1, 1);
    const std::size_t input = gate->registerInput();
    Finn::AsyncDeviceOutputBuffer<uint8_t> buffer(name, device, uuid, FinnUnittest::myShapePacked, FinnUnittest::parts, gate, ring);
    const auto waitForPosted = [&](uint32_t posted) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (xrt::ip::peek(name, ring.prod) < posted && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        return xrt::ip::peek(name, ring.prod);
    };
    const std::size_t batch = buffer.getTotalDataSize();

    gate->inputAnnounced(input);
    gate->inputAnnounced(input);
    EXPECT_EQ(waitForPosted(2), 2u);  // nothing has completed yet
    EXPECT_EQ(xrt::ip::peek(name, ring.frames), FinnUnittest::parts);
    EXPECT_EQ(xrt::ip::peek(name, ring.frames + 4), FinnUnittest::parts);

    xrt::ip::poke(name, ring.cons, 2);
    EXPECT_EQ(buffer.getData(2 * batch).size(), 2 * batch);
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(xrt::ip::peek(name, ring.prod), 2u);  // a batch the inputs did not announce is never posted

    gate->inputAnnounced(input);
    EXPECT_EQ(waitForPosted(3), 3u);
    xrt::ip::poke(name, ring.cons, 3);
    EXPECT_EQ(buffer.getData(batch).size(), batch);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}