#include <vector>

#define INSPECTION_TEST                 // For access to RingBuffer testing methods
#include "../../src/utils/RingBuffer.hpp"


#include "../../src/utils/Logger.h"
#include "../../src/utils/FinnUtils.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include <random>
#include <thread>
#include <algorithm>
#include <functional>


TEST(DummyTest, DT) {
    EXPECT_TRUE(true);
}


// Globals
using RB = RingBuffer<uint8_t>;
const size_t parts = 10;
const size_t elementsPerPart = 30;
auto filler = FinnUtils::BufferFiller(0,255);


TEST(RBTest, RBInitTest) {
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

TEST(RBTest, RBStoreTest) {
    auto rb = RB(parts, elementsPerPart);

    
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}