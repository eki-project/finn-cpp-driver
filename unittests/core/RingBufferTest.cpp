#include <vector>
#include "../../src/utils/RingBuffer.hpp"
#include "../../src/utils/Logger.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include <random>
#include <thread>
#include <algorithm>
#include <functional>


std::random_device rd;
std::mt19937 engine{rd()};
std::uniform_int_distribution<uint8_t> sampler(0, 0xFF);

using RB = RingBuffer<uint8_t>;

uint8_t generateRandomValue([[maybe_unused]] unsigned int unusedValue) {
    return sampler(engine);
}

// HELPER FUNC
inline void fillRandom(std::vector<uint8_t>& data) {
    std::transform(data.begin(), data.end(), data.begin(), generateRandomValue);
}

void randomizeAndFill(std::vector<uint8_t>& data, RB& rb) {
    fillRandom(data);
    rb.store(data);
}

std::vector<uint8_t> readData(RB& rb, index_t index) {
    return rb.getPart(index, false);
}

TEST(RingBufferTest, InitTest) {
    // Tools for testing

    // The test itself
    const size_t parts = 10;
    const size_t elementsPerPart = 30;
    auto rb = RB(parts, elementsPerPart);

    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS), parts * elementsPerPart);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::BYTES), parts * elementsPerPart * sizeof(uint8_t));
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::PARTS), parts);
    EXPECT_EQ(rb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART), elementsPerPart);

    EXPECT_EQ(rb.countValidParts(), 0);
    EXPECT_EQ(rb.getHeadOpposite(), 5);
    EXPECT_EQ(rb.getHeadIndex(), 0);
    EXPECT_FALSE(rb.isFull());
    EXPECT_FALSE(rb.isPreviousHalfValid());

    std::vector<uint8_t> data;
    data.resize(elementsPerPart);
    fillRandom(data);

    rb.store(data);

    EXPECT_EQ(rb.getHeadIndex(), 1);
    EXPECT_TRUE(rb.isPartValid(0));
    EXPECT_FALSE(rb.isPartValid(1));
    EXPECT_EQ(rb.getPart(0, true), data);
    EXPECT_EQ(rb.getHeadIndex(), 1);

    fillRandom(data);

    EXPECT_NE(rb.getPart(0, true), data);

    while (!rb.isPreviousHalfValid()) {
        fillRandom(data);
        rb.store(data);
    }


    // Test threading safety
    // TODO(bwintermann): Do proper tests here
    std::vector<std::thread> threadsVector;
    for (unsigned int i = 0; i < 1000; i++) {
        threadsVector.push_back(std::thread(randomizeAndFill, std::ref(data), std::ref(rb)));
        threadsVector.push_back(std::thread(readData, std::ref(rb), rb.getHeadOpposite()));
    }

    for (auto &t : threadsVector) {
        t.join();
    }
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}