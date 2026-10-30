/**
 * @file ConfigTest.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Unittest for the FINN Runtime config
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */
#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/Types.h>

#include "gtest/gtest.h"

TEST(ConfigTest, RingDmaEntry) {
    // the "ring" entry FINN's MakeCPPDriver writes for a free-running ring DMA
    const auto j = json::parse(R"({"kernelName": "StreamingDataflowPartition_0:{idma_0}", "normalShape": [1, 24], "foldedShape": [1, 1, 24],
        "packedShape": [1, 1, 24], "ring": {"slots": 16, "registers": {"addr_lo": 64, "addr_hi": 128, "frames": 192, "prod": 256, "cons": 264}}})");
    const auto ebd = j.get<Finn::ExtendedBufferDescriptor>();
    ASSERT_TRUE(ebd.ring.has_value());
    EXPECT_EQ(ebd.ring->slots, 16u);
    EXPECT_EQ(ebd.ring->addrLo, 64u);
    EXPECT_EQ(ebd.ring->addrHi, 128u);
    EXPECT_EQ(ebd.ring->frames, 192u);
    EXPECT_EQ(ebd.ring->prod, 256u);
    EXPECT_EQ(ebd.ring->cons, 264u);
    // round trip
    const json back = ebd;
    EXPECT_EQ(back.at("ring"), j.at("ring"));
}

TEST(ConfigTest, RegisterDmaEntry) {
    const auto j = json::parse(R"({"kernelName": "k", "normalShape": [1], "foldedShape": [1, 1], "packedShape": [1, 1]})");
    EXPECT_FALSE(j.get<Finn::ExtendedBufferDescriptor>().ring.has_value());
}

TEST(ConfigTest, RingDmaNeedsSlots) {
    const auto j = json::parse(R"({"slots": 0, "registers": {"addr_lo": 0, "addr_hi": 0, "frames": 0, "prod": 0, "cons": 0}})");
    EXPECT_THROW(j.get<Finn::RingControl>(), std::runtime_error);
}

/*
TEST(ConfigTest, JsonConversion) {
    EXPECT_NO_THROW(auto j = json::parse(R"({"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]})"););
    EXPECT_NO_THROW(auto j = json::parse(R"({"idmas":[{"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]}], "odmas":[], "xclbinPath":"test", "name":"test"})"););
}
*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}