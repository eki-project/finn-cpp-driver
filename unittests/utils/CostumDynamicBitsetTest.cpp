#include <utils/CostumDynamicBitset.h>

#include <algorithm>
#include <utils/join.hpp>

#include "gtest/gtest.h"


TEST(CostumDynamicBitsetTest, SettingBits) {
    std::string testString = "0000000000000000000000000000000000000000000000000000001000110001";
    for (std::size_t i = 0; i <= 53; ++i) {
        DynamicBitset set(64);
        uint32_t x = 1;
        x |= (1 << 4);
        set.setByte(x, i);
        set.setByte(x, i + 5);
        EXPECT_EQ(testString, set.to_string());
        std::rotate(testString.begin(), testString.begin() + 1, testString.end());
    }
}

TEST(CostumDynamicBitsetTest, OutputTest) {
    DynamicBitset set(64);
    uint32_t x = 1;
    x |= (1 << 4);
    set.setByte(x, 0);
    x = x << 2;
    set.setByte(x, 0);
    std::vector<uint8_t> out;
    set.outputBytes(std::back_inserter(out));
    std::vector<uint8_t> base = {0, 0, 0, 0, 0, 0, 0, 85};
}

TEST(CostumDynamicBitsetTest, MergeTest) {
    std::string testString = "0000000000000000000000000000000000000000000000000000000001010101";

    DynamicBitset set(64);
    uint32_t x = 1;
    x |= (1 << 4);
    set.setByte(x, 0);
    x = x << 2;
    DynamicBitset set2(64);
    set2.setByte(x, 0);
    set2 |= set;
    EXPECT_EQ(testString, set2.to_string());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}