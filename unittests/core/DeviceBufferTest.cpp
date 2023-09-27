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

TEST(DeviceBufferTest, CorrectOutputTest) {
    // Setup
    auto filler = FinnUtils::BufferFiller(0,255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto odb = Finn::DeviceOutputBuffer<uint8_t>("tester", device, kernel, myShapePacked, parts);

    std::vector<uint8_t> data;
    std::vector<uint8_t> backupData;
    data.resize(odb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    // Fill with random data and backup
    filler.fillRandom(data);
    backupData = data;

    // Write data -> Map
    odb.testSetMap(data);
    EXPECT_EQ(data, odb.testGetMap());

    // Move Map -> Buffer[0]
    EXPECT_EQ(odb.read(1), ERT_CMD_STATE_COMPLETED);
    EXPECT_EQ(odb.testGetRingBuffer().testGetAsVector(0), backupData);

    // Write Buffer[0] -> LTS[0]
    EXPECT_EQ(odb.testGetRingBuffer().countValidParts(), 1);
    odb.archiveValidBufferParts();

    // Test that the data arrived in the lts
    EXPECT_EQ(odb.testGetLTS().size(), 1);
    EXPECT_EQ(odb.testGetLTS()[0], backupData);

    // Test that the LTS gets fetched and deleted as wanted
    EXPECT_EQ(odb.retrieveArchive()[0], backupData);
    EXPECT_EQ(odb.testGetLTS().size(), 0);
}

TEST(DeviceBufferTest, CorrectInputTest) {
    auto filler = FinnUtils::BufferFiller(0,255);
    auto device = xrt::device();
    auto kernel = xrt::kernel();
    auto idb = Finn::DeviceInputBuffer<uint8_t>("tester", device, kernel, myShapePacked, parts);

    std::vector<uint8_t> data;
    std::vector<uint8_t> backupData;
    data.resize(idb.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));

    // Fill with random data
    filler.fillRandom(data);
    backupData = data;

    // Check that after the storing, the data is in the ring buffer (Data -> Buffer[0])
    EXPECT_NE(idb.testGetRingBuffer().testGetAsVector(0), data);
    idb.store(data);
    EXPECT_EQ(idb.testGetRingBuffer().countValidParts(), 1);
    EXPECT_EQ(idb.testGetRingBuffer().testGetAsVector(0), data);

    // Check that data is in the map (Buffer[0] -> Map) and invalid
    idb.loadMap();
    EXPECT_EQ(idb.testGetMap(), data);
    EXPECT_EQ(idb.testGetRingBuffer().countValidParts(), 0);

}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}