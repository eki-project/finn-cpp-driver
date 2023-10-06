#ifndef COSTUMDYNAMICBITSET_H
#define COSTUMDYNAMICBITSET_H

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <type_traits>
#include <vector>

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
    ~DynamicBitset() = default;

    std::size_t size() { return bits.size() * 8; }

#pragma omp declare simd
    void set(std::size_t n) {
        std::size_t index = n / 8;
        std::size_t bit = n % 8;
        bits[index] |= static_cast<uint8_t>(1U << bit);
    }

    // TODO(linusjun): Testen mit uint64_t! Optimieren!
    template<typename T>
    void set2(const T x, std::size_t n) {
        std::array<uint8_t, sizeof(T)> input;
        std::size_t nOffset = n % 8;
        std::size_t byte = n / 8;
        if (nOffset != 0 && ((static_cast<T>(0xFF) << (sizeof(T) * 8 - nOffset)) & x) != 0) {
            // handle overflow nach links beim shiften
            T overflow = ((static_cast<T>(0xFF) << (sizeof(T) * 8 - nOffset)) & x);
            uint8_t overflowConverted = static_cast<uint8_t>(overflow >> (sizeof(T) * 8 - nOffset));
            bits[byte + input.size()] |= overflowConverted;
        }
        T x2 = x << nOffset;
        std::memcpy(input.data(), &x2, sizeof(T));


        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i] != 0) {
                bits[byte + i] |= input[i];
            }
        }
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

#endif  // COSTUMDYNAMICBITSET_H
