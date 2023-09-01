#include <random>
#include <span>

#include "../../src/core/DeviceBuffer.hpp"
#include "../../src/utils/FinnDatatypes.hpp"

#include "gtest/gtest.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_device.h"

#include "../../src/utils/Logger.h"
//using namespace Finn;



constexpr std::array<unsigned int, 4> myShapeArray = std::array<unsigned int, 4>{1,28,28,3};
constexpr size_t elementsPerPart = 1*28*28*3;
constexpr size_t parts = 10;

TEST(DeviceBufferTest, DBInitTest) {
    auto log = Logger::getLogger();

    FINN_LOG(log, loglevel::debug) << "Starting DeviceBufferTest…\n";
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    shape_t myShape = std::vector<unsigned int>(myShapeArray.begin(), myShapeArray.end());
    std::string myname = "abcd";

    FINN_LOG(log, loglevel::debug) << "Creating DeviceInputBuffer\n";
    auto inputDB = Finn::DeviceInputBuffer<uint8_t, DatatypeUInt<2>>(myname, device, kernel, myShape, parts);

    EXPECT_EQ(FinnUtils::shapeToElements(myShape), elementsPerPart);

    FINN_LOG(log, loglevel::debug) << "Size tests\n";
    EXPECT_EQ(inputDB.getHeadOpposideIndex(), 5);
    EXPECT_FALSE(inputDB.isExecutedAutomatically());
    EXPECT_EQ(inputDB.size(SIZE_SPECIFIER::PARTS), 10);
    EXPECT_EQ(inputDB.size(SIZE_SPECIFIER::ELEMENTS_PER_PART), FinnUtils::shapeToElements(myShape));
    EXPECT_EQ(inputDB.size(SIZE_SPECIFIER::ELEMENTS), 10 * FinnUtils::shapeToElements(myShape));
    EXPECT_EQ(inputDB.size(SIZE_SPECIFIER::BYTES), sizeof(uint8_t) * 10 * FinnUtils::shapeToElements(myShape));

    FINN_LOG(log, loglevel::debug) << "Testing that all parts are initialized invalid\n";
    for (index_t i = 0; i < inputDB.size(SIZE_SPECIFIER::PARTS); i++) {
        EXPECT_EQ(inputDB.isPartValid(i), false);
    }
}

TEST(DeviceBufferTest, DBWriteReadTest) {
    auto log = Logger::getLogger();
    std::random_device rd;
    std::mt19937 engine{rd()};
    std::uniform_int_distribution<uint8_t> sampler(0, 0xFF);

    FINN_LOG(log, loglevel::debug) << "Starting DBWRiteReadTest\n";
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    shape_t myShape = std::vector<unsigned int>(myShapeArray.begin(), myShapeArray.end());
    std::string myname = "abcd";

    FINN_LOG(log, loglevel::debug) << "Creating DeviceInputBuffer\n";
    auto inputDB = Finn::DeviceInputBuffer<uint8_t, DatatypeUInt<2>>(myname, device, kernel, myShape, parts);

    EXPECT_EQ(FinnUtils::shapeToElements(myShape), elementsPerPart);

    // Create random data for all parts
    std::array<std::array<uint8_t, elementsPerPart>, parts> randomData;
    for (size_t i = 0; i < parts; i++) {
        randomData[i] = std::array<uint8_t, elementsPerPart>();
    }

    for (auto &arr : randomData) {
        // NOLINTNEXTLINE
        std::transform(arr.begin(), arr.end(), arr.begin(), [&sampler, &engine](uint8_t x){ return (x-x) + sampler(engine); });
    }

    for (size_t i = 0; i < parts-1; i++) {
        EXPECT_EQ(inputDB.getHeadIndex(), i);
        inputDB.store<elementsPerPart>(randomData[i]);
        EXPECT_FALSE(inputDB.isHeadValid());
    }
    EXPECT_EQ(inputDB.getHeadIndex(), parts-1);

    inputDB.store<elementsPerPart>(randomData[9]);
    EXPECT_TRUE(inputDB.isHeadValid());
    EXPECT_EQ(inputDB.getHeadIndex(), 0);
    

    inputDB.store<elementsPerPart>(randomData[9]);
    EXPECT_TRUE(inputDB.isHeadValid());
    EXPECT_EQ(inputDB.getHeadIndex(), 1);
    EXPECT_TRUE(inputDB.isPartValid(0));

    inputDB.setExecuteAutomatically(true);
    EXPECT_TRUE(inputDB.isExecutedAutomatically());

    inputDB.store<elementsPerPart>(randomData[9]);
    EXPECT_FALSE(inputDB.isHeadValid());
    EXPECT_TRUE(inputDB.isPartValid(1));
    EXPECT_EQ(inputDB.get<elementsPerPart>(1), randomData[9]);
    EXPECT_EQ(inputDB.get<elementsPerPart>(2), randomData[2]);
    EXPECT_FALSE(inputDB.isPartValid(2));

}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}