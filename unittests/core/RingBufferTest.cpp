/**
 * @file RingBufferTest.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Unittest for the Ring Buffer
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Logger.h>

#include <FINNCppDriver/utils/RingBuffer.hpp>
#include <algorithm>
#include <functional>
#include <random>
#include <thread>
#include <vector>

#include "UnittestConfig.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"


TEST(DummyTest, DT) { EXPECT_TRUE(true); }


// Globals
using RB = RingBuffer<uint8_t>;
const size_t parts = FinnUnittest::parts;
const size_t elementsPerPart = FinnUnittest::elementsPerPart;


class RBTest : public ::testing::Test {
     protected:
    RB rb = RB(parts, elementsPerPart);
    Finn::vector<uint8_t> data;
    std::vector<Finn::vector<uint8_t>> storedDatas;
    FinnUtils::BufferFiller filler = FinnUtils::BufferFiller(0, 255);
    void SetUp() override { data.resize(rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)); }

    /**
     * @brief Utility function to completely fill a ringBuffer or a deviceinput/output buffer.
     *
     * This function uses the data vector to fill the entire rb of type T with random data, based on it's size.
     * storedDatas gets all data used pushed back.
     *
     * @param fast Whether to use fast store methods (no mutex locking, no length checks)
     * @param ref Whether to use references (true) or iterators (false)
     */
    void fillCompletely(bool fast, bool ref) {
        for (size_t i = 0; i < rb.size(SIZE_SPECIFIER::PARTS); i++) {
            filler.fillRandom(data.begin(), data.end());
            storedDatas.push_back(data);
            if (fast) {
                if (ref) {
                    EXPECT_TRUE(rb.storeFast(data));
                } else {
                    EXPECT_TRUE(rb.storeFast(data.begin(), data.end()));
                }
            } else {
                if (ref) {
                    EXPECT_TRUE(rb.store(data, data.size()));
                } else {
                    EXPECT_TRUE(rb.store(data.begin(), data.end()));
                }
            }
        }
    }

    void TearDown() override {}
};


TEST(RBTestManual, RBInitTest) {
    auto rb = RB(parts, elementsPerPart);

    // Pointers
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);
    EXPECT_FALSE(rb.testGetValidity(0));

    // Sizes
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::PARTS), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART), elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), parts * elementsPerPart * 1);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS), parts * elementsPerPart);

    // Initial values
    EXPECT_EQ(rb.countValidParts(), 0);
    for (auto elem : rb.testGetAsVector(0)) {
        EXPECT_EQ(elem, 0);
    }
    EXPECT_FALSE(rb.isFull());
}

TEST_F(RBTest, RBStoreReadTestIterator) {
    fillCompletely(false, false);

    // Confirm that the head pointer wrapped around
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Temporary save first entry
    auto current = rb.testGetAsVector(0);

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.store(data.begin(), data.end()));

    // Test that the valid data was not changed
    EXPECT_EQ(rb.testGetAsVector(0), current);

    // Read two entries
    uint8_t* buf = new uint8_t[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check pointer positions
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 2);
    delete[] buf;
}

TEST_F(RBTest, RBFastStoreTestIterator) {
    fillCompletely(true, false);

    // Confirm that the head pointer wrapped around
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Temporary save first entry
    auto current = rb.testGetAsVector(0);

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.storeFast(data.begin(), data.end()));

    // Test that the valid data was not changed
    EXPECT_EQ(rb.testGetAsVector(0), current);

    // Read two entries
    uint8_t* buf = new uint8_t[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check pointer positions
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 2);
    delete[] buf;
}

TEST_F(RBTest, RBStoreReadTestReference) {
    fillCompletely(false, true);

    // Confirm that the head pointer wrapped around
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Temporary save first entry
    auto current = rb.testGetAsVector(0);

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.store(data, data.size()));

    // Test that the valid data was not changed
    EXPECT_EQ(rb.testGetAsVector(0), current);

    // Read two entries
    uint8_t* buf = new uint8_t[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check pointer positions
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 2);
    delete[] buf;
}

TEST_F(RBTest, RBFastStoreTestReference) {
    fillCompletely(true, true);

    // Confirm that the head pointer wrapped around
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Temporary save first entry
    auto current = rb.testGetAsVector(0);

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.storeFast(data));

    // Test that the valid data was not changed
    EXPECT_EQ(rb.testGetAsVector(0), current);

    // Read two entries
    uint8_t* buf = new uint8_t[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check pointer positions
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 2);
    delete[] buf;
}

TEST_F(RBTest, RBReadTest) {
    //* Requires that the store tests ran successfully to be successfull itself
    fillCompletely(true, true);
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Check that the read data is equivalent to the saved data and read in the same order (important!)
    for (unsigned int i = 0; i < rb.size(SIZE_SPECIFIER::PARTS); i++) {
        EXPECT_TRUE(rb.read(data.begin()));
        EXPECT_EQ(storedDatas[i], data);
    }
}

TEST_F(RBTest, RBReadTestArray) {
    //* Requires that the store tests ran successfully to be successfull itself
    fillCompletely(true, true);
    EXPECT_EQ(rb.testGetHeadPointer(), 0);
    EXPECT_EQ(rb.testGetReadPointer(), 0);

    // Check that the read data is equivalent to the saved data and read in the same order (important!)
    uint8_t* buf = new uint8_t[rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)];
    for (unsigned int i = 0; i < rb.size(SIZE_SPECIFIER::PARTS); i++) {
        EXPECT_TRUE(rb.read(buf));
        for (unsigned int j = 0; j < rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART); j++) {
            EXPECT_EQ(storedDatas[i][j], buf[j]);
        }
    }
    delete[] buf;
}

TEST_F(RBTest, RBUtilFuncsTest) {
    // Check all sizes
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::PARTS), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART), elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), elementsPerPart * 1 * parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS), elementsPerPart * parts);

    // Check validity flags
    fillCompletely(true, true);
    EXPECT_TRUE(rb.isFull());
    EXPECT_EQ(rb.countValidParts(), rb.size(SIZE_SPECIFIER::PARTS));

    EXPECT_TRUE(rb.read(data.begin()));
    EXPECT_FALSE(rb.isFull());
    EXPECT_EQ(rb.countValidParts(), rb.size(SIZE_SPECIFIER::PARTS) - 1);
}

TEST_F(RBTest, RBValidityTest) {
    for (unsigned int i = 0; i < 3; i++) {
        filler.fillRandom(data.begin(), data.end());
        EXPECT_TRUE(rb.storeFast(data));
    }

    // Check that the correct parts are set to false
    EXPECT_FALSE(rb.isFull());
    EXPECT_EQ(rb.countValidParts(), 3);
    EXPECT_TRUE(rb.testGetValidity(0));
    EXPECT_TRUE(rb.testGetValidity(1));
    EXPECT_TRUE(rb.testGetValidity(2));
    for (unsigned int j = 3; j < rb.size(SIZE_SPECIFIER::PARTS); j++) {
        EXPECT_FALSE(rb.testGetValidity(j));
    }

    rb.setPartValidityMutexed(0, false);
    EXPECT_FALSE(rb.testGetValidity(0));

    rb.setPartValidityMutexed(3, true);
    EXPECT_TRUE(rb.testGetValidity(3));

    //! Run test twice to see that the value does not get flipped instead of set after a set call
    rb.setPartValidityMutexed(0, false);
    EXPECT_FALSE(rb.testGetValidity(0));

    rb.setPartValidityMutexed(3, true);
    EXPECT_TRUE(rb.testGetValidity(3));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}