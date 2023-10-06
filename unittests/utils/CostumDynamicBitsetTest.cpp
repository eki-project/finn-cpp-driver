#include <utils/CostumDynamicBitset.h>

#include "gtest/gtest.h"


TEST(CostumDynamicBitsetTest, Dummy) {
    for (std::size_t i = 0; i <= 50; ++i) {
        DynamicBitset set(64);
        uint32_t x = 1;
        x |= (1 << 4);
        set.set2(x, i);
        x = x << 2;
        set.set2(x, i);
        std::cout << set.to_string() << "\n";
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}