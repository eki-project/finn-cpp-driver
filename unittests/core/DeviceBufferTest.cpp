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

#include <FINNCppDriver/core/DeviceBuffer/SyncDeviceBuffers.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <memory>
#include <random>
#include <span>

#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Provides config and shapes for testing
#include "UnittestConfig.h"
using namespace FinnUnittest;

class DBTest : public ::testing::Test {
     protected:
    xrt::device device;
    xrt::kernel kernel;
    Finn::SyncDeviceInputBuffer<uint8_t> buffer = Finn::SyncDeviceInputBuffer<uint8_t>("TestBuffer", device, kernel, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::SyncDeviceOutputBuffer<uint8_t> outputBuffer = Finn::SyncDeviceOutputBuffer<uint8_t>("tester", device, kernel, FinnUnittest::myShapePacked, FinnUnittest::parts);
    FinnUtils::BufferFiller filler = FinnUtils::BufferFiller(0, 255);
    std::vector<Finn::vector<uint8_t>> storedDatas;
    Finn::vector<uint8_t> data;
    size_t bufferParts;
    size_t bufferElemPerPart;
    void SetUp() override {
        std::cout << "ELEMENTS PER PART " << buffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART) << "\n";
        std::cout << "data size: " << data.size() << "\n";
        data.resize(buffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
        bufferParts = buffer.size(SIZE_SPECIFIER::PARTS);
        bufferElemPerPart = buffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
        std::cout << "data size post setup: " << data.size() << "\n";
    }

    /**
     * @brief Utility function to completely fill a ringBuffer or a deviceinput/output buffer.
     *
     * This function uses the data vector to fill the entire rb of type T with random data, based on it's size.
     * storedDatas gets all data used pushed back.
     *
     * @param ref Whether to use references (true) or iterators (false)
     * @param isInput If true use the input buffer, else the output buffer
     */
    void fillCompletely(bool ref) {
        for (size_t i = 0; i < buffer.size(SIZE_SPECIFIER::PARTS); i++) {
            filler.fillRandom(data.begin(), data.end());
            storedDatas.push_back(data);

            if (ref) {
                EXPECT_TRUE(buffer.store(data));
            } else {
                EXPECT_TRUE(buffer.store(data.begin(), data.end()));
            }
        }
    }

    /**
     * @brief Utility function to read out every part of the input buffer (writing it into map, this also checks loading) and checks for order and correctness
     *
     */
    void readAndCompare() {
        for (unsigned int part = 0; part < bufferParts; part++) {
            buffer.loadMap();
            EXPECT_EQ(storedDatas[part], buffer.testGetMap());
        }
    }

    void clearStoredDatas() { storedDatas.resize(0); }

    void TearDown() override {}
};

auto filler = FinnUtils::BufferFiller(0, 255);

TEST_F(DBTest, DBStoreLoadMapTest) {
    auto initialMapData = buffer.testGetMap();
    //* Test if data is correctly put into the memory buffer

    // Iterator
    fillCompletely(false);
    readAndCompare();
    clearStoredDatas();

    // Reference
    fillCompletely(true);
    readAndCompare();
    clearStoredDatas();
}

TEST_F(DBTest, DBOutputTest) {
    // Shape
    EXPECT_EQ(outputBuffer.getPackedShape(), FinnUnittest::myShapePacked);

    // LTS Allocation
    outputBuffer.allocateLongTermStorage(100);
    EXPECT_EQ(outputBuffer.testGetLTS().capacity(), 100 * outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    // Test that timeout is set to a good default value
    EXPECT_EQ(outputBuffer.getMsExecuteTimeout(), 1000);

    std::cout << "Data size: " << data.size() << std::endl;

    // Test data in and out readout
    filler.fillRandom(data.begin(), data.end());
    storedDatas.push_back(data);
    outputBuffer.testSetMap(data);
    outputBuffer.saveMap();

    outputBuffer.testGetRingBuffer().read(data.begin());
    EXPECT_EQ(data, storedDatas[0]);
}

TEST_F(DBTest, DBLTSTest) {
    // Write too many entries into the ring buffer
    for (unsigned int i = 0; i < outputBuffer.size(SIZE_SPECIFIER::PARTS) + 2; i++) {
        filler.fillRandom(data.begin(), data.end());
        storedDatas.push_back(data);
        outputBuffer.testSetMap(data);

        outputBuffer.read(1);
    }

    // Expected: Buffer was full at max capacity. All entries were valid and put into the LTS, and the two new entries are in the buffer
    EXPECT_FALSE(outputBuffer.testGetRingBuffer().full());
    EXPECT_EQ(outputBuffer.testGetRingBuffer().size(), 2);
    EXPECT_EQ(outputBuffer.testGetLTS().size(), outputBuffer.size(SIZE_SPECIFIER::PARTS) * outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    // Check integrity of data
    auto results = outputBuffer.retrieveArchive();
    auto startIt = results.begin();
    for (unsigned int i = 0; i < results.size() / outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART); i++) {
        EXPECT_TRUE(
            std::equal(startIt + (i * static_cast<long int>(outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART))), startIt + ((i + 1) * static_cast<long int>(outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART))), storedDatas[i].begin()));
    }

    // Check the data that is still in the buffer
    Finn::vector<uint8_t> vec;
    outputBuffer.testGetRingBuffer().readWithoutInvalidation(std::back_inserter(vec), 0);
    EXPECT_EQ(storedDatas[outputBuffer.size(SIZE_SPECIFIER::PARTS)], vec);
    vec.clear();
    outputBuffer.testGetRingBuffer().readWithoutInvalidation(std::back_inserter(vec), 1);
    EXPECT_EQ(storedDatas[outputBuffer.size(SIZE_SPECIFIER::PARTS) + 1], vec);

    // Check that the LTS was cleared
    EXPECT_EQ(outputBuffer.testGetLTS().size(), 0);

    // Manual transfer to LTS
    outputBuffer.archiveValidBufferParts();
    EXPECT_EQ(outputBuffer.testGetLTS().size(), 2 * outputBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
    EXPECT_EQ(outputBuffer.testGetRingBuffer().size(), 0);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}