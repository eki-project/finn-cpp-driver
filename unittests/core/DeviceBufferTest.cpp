#include <random>
#include <span>

#include "../../src/core/DeviceBuffer.hpp"
#include "../../src/utils/FinnDatatypes.hpp"
#include "../../src/utils/Logger.h"
#include "gtest/gtest.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
// using namespace Finn;

TEST(DummyTestDB, DTDB) { EXPECT_TRUE(true); }


constexpr std::array<unsigned int, 2> myShapeArrayNormal = std::array<unsigned int, 2>{1, 300};
constexpr std::array<unsigned int, 3> myShapeArrayFolded = std::array<unsigned int, 3>{1, 10, 30};
constexpr std::array<unsigned int, 3> myShapeArrayPacked = std::array<unsigned int, 3>{1, 10, 8};    // This packing assumes Uint2 as finn datatype
constexpr std::array<unsigned int, 3> myShapeArrayPacked2 = std::array<unsigned int, 3>{1, 10, 53};  // This packing assumes Uint14 as finn datatype
constexpr size_t elementsPerPart = FinnUtils::shapeToElementsConstexpr<unsigned int, 3>(myShapeArrayPacked);
constexpr size_t parts = 10;

shape_t myShapeNormal = std::vector<unsigned int>(myShapeArrayNormal.begin(), myShapeArrayNormal.end());
shape_t myShapeFolded = std::vector<unsigned int>(myShapeArrayFolded.begin(), myShapeArrayFolded.end());
shape_t myShapePacked = std::vector<unsigned int>(myShapeArrayPacked.begin(), myShapeArrayPacked.end());

TEST(DeviceBufferTest, BasicFunctionalityTest) {
    auto log = Logger::getLogger();
    std::random_device rd;
    std::mt19937 engine{rd()};
    std::uniform_int_distribution<uint8_t> sampler(0, 0xFF);

    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto inputDB = Finn::DeviceInputBuffer<uint8_t /*, DatatypeUInt<2>*/>("test", device, kernel, /*myShapeNormal, myShapeFolded,*/ myShapePacked, parts);
    std::vector<uint8_t> data(inputDB.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    auto initialMapData = inputDB.testGetMap();

    // Func to fill data with random values
    auto fillRandomData = [&sampler, &engine, &data]() { std::transform(data.begin(), data.end(), data.begin(), [&sampler, &engine]([[maybe_unused]] uint8_t _) { return sampler(engine); }); };

    // Test basic functions
    fillRandomData();
    inputDB.store(data);
    EXPECT_EQ(inputDB.testGetRingBuffer().testGetAsVector(0), data);

    inputDB.loadMap();
    EXPECT_NE(initialMapData, inputDB.testGetMap());
    EXPECT_EQ(inputDB.testGetMap(), data);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}