#include <random>
#include <span>

#include "../../src/core/DeviceBuffer.hpp"
#include "../../src/utils/FinnDatatypes.hpp"
#include "../../src/utils/Logger.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Provides config and shapes for testing
#include "UnittestConfig.h"
using namespace FinnUnittest;

/**
 * @brief Utility function to completely fill a ringBuffer or a deviceinput/output buffer. 
 * 
 * This function uses the data vector to fill the entire rb of type T with random data, based on it's size. 
 * storedDatas gets all data used pushed back.
 * 
 * @tparam T Must be one of the above mentioned types 
 * @param rb 
 * @param data 
 * @param storedDatas 
 * @param fast Whether to use fast store methods (no mutex locking, no length checks) 
 * @param ref Whether to use references (true) or iterators (false)
 * @param pfiller 
 */
template<typename T>
void fillCompletely(T& rb, std::vector<uint8_t>& data, std::vector<std::vector<uint8_t>>& storedDatas, bool fast, bool ref, FinnUtils::BufferFiller& pfiller) {
    for (size_t i = 0; i < rb.size(SIZE_SPECIFIER::PARTS); i++) {
        pfiller.fillRandom(data);
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

class DBTest : public ::testing::Test {
     protected:
    xrt::device device;
    xrt::kernel kernel;
    Finn::DeviceInputBuffer<uint8_t> buffer = Finn::DeviceInputBuffer<uint8_t>("TestBuffer", device, kernel, FinnUnittest::myShapePacked, FinnUnittest::parts);
    Finn::DeviceOutputBuffer<uint8_t> outputBuffer = Finn::DeviceOutputBuffer<uint8_t>("tester", device, kernel, FinnUnittest::myShapePacked, FinnUnittest::parts);
    std::vector<uint8_t> data;
    void SetUp() override {
        device = xrt::device();
        kernel = xrt::kernel();
    }


    void TearDown() override {}
};

auto filler = FinnUtils::BufferFiller(0,255);


TEST_F(DBTest, DBStoreTest) {
    auto initialMapData = buffer.testGetMap();

    auto bufferParts = buffer.size(SIZE_SPECIFIER::PARTS);
    auto bufferElemPerPart = buffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);


    
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}