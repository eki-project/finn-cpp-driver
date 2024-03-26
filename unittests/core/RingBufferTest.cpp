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

// Globals
using RB = Finn::RingBuffer<int, false>;
const size_t parts = FinnUnittest::parts;
// const size_t elementsPerPart = FinnUnittest::elementsPerPart;
const size_t elementsPerPart = 5;


class RBTest : public ::testing::Test {
     protected:
    RB rb = RB(parts, elementsPerPart);
    Finn::vector<int> data;
    std::vector<Finn::vector<int>> storedDatas;
    FinnUtils::BufferFiller filler = FinnUtils::BufferFiller(0, 255);
    void SetUp() override { data.resize(rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE)); }

    /**
     * @brief Utility function to completely fill a ringBuffer or a deviceinput/output buffer.
     *
     * This function uses the data vector to fill the entire rb of type T with random data, based on it's size.
     * storedDatas gets all data used pushed back.
     *
     * @param fast Whether to use fast store methods (no mutex locking, no length checks)
     * @param ref Whether to use references (true) or iterators (false)
     */
    void fillCompletely(bool ref) {
        for (size_t i = 0; i < rb.size(SIZE_SPECIFIER::BATCHSIZE); i++) {
            filler.fillRandom(data.begin(), data.end());
            storedDatas.push_back(data);

            if (ref) {
                EXPECT_TRUE(rb.store(data.begin(), data.size()));
            } else {
                EXPECT_TRUE(rb.store(data.begin(), data.end()));
            }
        }
    }

    void TearDown() override {}
};

class RBTestBlocking : public ::testing::Test {
     protected:
    Finn::RingBuffer<int, true> rb = Finn::RingBuffer<int, true>(parts, elementsPerPart);
    Finn::vector<int> data;
    std::vector<Finn::vector<int>> storedDatas;
    FinnUtils::BufferFiller filler = FinnUtils::BufferFiller(0, 255);
    void SetUp() override { data.resize(rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE)); }

    /**
     * @brief Utility function to completely fill a ringBuffer or a deviceinput/output buffer.
     *
     * This function uses the data vector to fill the entire rb of type T with random data, based on it's size.
     * storedDatas gets all data used pushed back.
     *
     * @param fast Whether to use fast store methods (no mutex locking, no length checks)
     * @param ref Whether to use references (true) or iterators (false)
     */
    void fillCompletely(bool ref) {
        for (size_t i = 0; i < rb.size(SIZE_SPECIFIER::BATCHSIZE); i++) {
            filler.fillRandom(data.begin(), data.end());
            storedDatas.push_back(data);

            if (ref) {
                EXPECT_TRUE(rb.store(data.begin(), data.size()));
            } else {
                EXPECT_TRUE(rb.store(data.begin(), data.end()));
            }
        }
    }

    void TearDown() override {}
};


TEST(RBTestManual, RBInitTest) {
    auto rb = RB(parts, elementsPerPart);

    // Pointers
    EXPECT_TRUE(rb.empty());

    // Sizes
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BATCHSIZE), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE), elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), parts * elementsPerPart * sizeof(int));
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::TOTAL_DATA_SIZE), parts * elementsPerPart);

    // Initial values
    std::vector<int> out;
    rb.readAllValidParts(std::back_inserter(out));
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(rb.full());
}

TEST_F(RBTest, RBStoreReadTestIterator) {
    fillCompletely(false);

    // Temporary save entries
    std::vector<int> current;
    rb.readWithoutInvalidation(std::back_inserter(current));

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.store(data.begin(), data.end()));

    // Test that the valid data was not changed
    std::vector<int> after;
    rb.readWithoutInvalidation(std::back_inserter(after));
    EXPECT_EQ(after, current);

    // Read two entries
    std::size_t oldSize = rb.size();
    int* buf = new int[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check size
    EXPECT_EQ(rb.size(), oldSize - 2);
    delete[] buf;
}

TEST_F(RBTestBlocking, RBFastStoreTestIterator) {
    fillCompletely(true);

    // Read two entries
    std::size_t oldSize = rb.size();
    int* buf = new int[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check size
    EXPECT_EQ(rb.size(), oldSize - 2);
    delete[] buf;
}

TEST_F(RBTest, RBStoreReadTestReference) {
    fillCompletely(true);

    // Temporary save entries
    std::vector<int> current;
    rb.readWithoutInvalidation(std::back_inserter(current));

    // Confirm that no new data can be stored until some data is read
    filler.fillRandom(data.begin(), data.end());
    EXPECT_FALSE(rb.store(data.begin(), data.end()));

    // Test that the valid data was not changed
    std::vector<int> after;
    rb.readWithoutInvalidation(std::back_inserter(after));
    EXPECT_EQ(after, current);

    // Read two entries
    std::size_t oldSize = rb.size();
    int* buf = new int[elementsPerPart];
    EXPECT_TRUE(rb.read(buf));
    EXPECT_TRUE(rb.read(buf));

    // Check size
    EXPECT_EQ(rb.size(), oldSize - 2);
    delete[] buf;
}

TEST_F(RBTest, RBReadTest) {
    //* Requires that the store tests ran successfully to be successfull itself
    fillCompletely(true);

    // Check that the read data is equivalent to the saved data and read in the same order (important!)
    for (unsigned int i = 0; i < rb.size(SIZE_SPECIFIER::BATCHSIZE); i++) {
        EXPECT_TRUE(rb.read(data.begin()));
        EXPECT_EQ(storedDatas[i], data);
    }
}

TEST_F(RBTest, RBReadTestArray) {
    //* Requires that the store tests ran successfully to be successfull itself
    fillCompletely(true);

    // Check that the read data is equivalent to the saved data and read in the same order (important!)
    int* buf = new int[rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE)];
    for (unsigned int i = 0; i < rb.size(SIZE_SPECIFIER::BATCHSIZE); i++) {
        EXPECT_TRUE(rb.read(buf));
        for (unsigned int j = 0; j < rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE); j++) {
            EXPECT_EQ(storedDatas[i][j], buf[j]);
        }
        break;
    }
    delete[] buf;
}

TEST_F(RBTest, RBUtilFuncsTest) {
    // Check all sizes
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BATCHSIZE), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::FEATUREMAP_SIZE), elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), elementsPerPart * sizeof(int) * parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::TOTAL_DATA_SIZE), elementsPerPart * parts);

    // Check validity flags
    fillCompletely(true);
    EXPECT_TRUE(rb.full());
    EXPECT_EQ(rb.size(), rb.size(SIZE_SPECIFIER::BATCHSIZE));

    EXPECT_TRUE(rb.read(data.begin()));
    EXPECT_FALSE(rb.full());
    EXPECT_EQ(rb.size(), rb.size(SIZE_SPECIFIER::BATCHSIZE) - 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}