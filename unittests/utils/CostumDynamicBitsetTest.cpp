#include <cstdint>
#include <sstream>

#include "gtest/gtest.h"

class DynamicBitset {
     public:
    std::vector<uint8_t> bits;
    DynamicBitset(std::size_t n) {
        size_t allocBytes = ((n / 8) + ((n % 8 != 0) ? 1 : 0));
        bits.resize(allocBytes);
    }
    DynamicBitset(DynamicBitset&&) = default;
    DynamicBitset(const DynamicBitset&) = default;
    DynamicBitset& operator=(DynamicBitset&&) = default;
    DynamicBitset& operator=(const DynamicBitset&) = default;
    ~DynamicBitset();

#pragma omp declare simd
    void set(std::size_t n) {
        std::size_t index = n / 8;
        std::size_t bit = n % 8;
        bits[index] |= static_cast<uint8_t>(1U << bit);
    }

    std::string to_string() {
        std::stringstream out;
        for (int i = bits.size() - 1; i >= 0; --i) {
            for (std::size_t j = 0; j < 8; ++j) {
                out << (0 != (bits[i] & (128U >> j)));
            }
        }
        return out.str();
    }

     private:
};

DynamicBitset::~DynamicBitset() {}

TEST(CostumDynamicBitsetTest, Dummy) {
    DynamicBitset set(64);
#pragma omp simd
    for (std::size_t i = 0; i < 9; ++i) {
        set.set(i);
    }
    std::cout << set.to_string() << "\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}