#ifndef DATAPACKING_HPP
#define DATAPACKING_HPP

#include <algorithm>
#include <bit>
#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <vector>

#include "FinnDatatypes.hpp"
#include "FinnUtils.h"
#include "join.hpp"

namespace Finn {
    /**
     * @brief Namespace to extremely efficiently and fast flip the bits in a number.
     *
     */
    namespace bitshuffling {
        constexpr std::array<uint8_t, 256> table{
            0,   128, 64,  192, 32,  160, 96,  224, 16,  144, 80,  208, 48,  176, 112, 240, 8,   136, 72,  200, 40,  168, 104, 232, 24,  152, 88,  216, 56,  184, 120, 248, 4,   132, 68,  196, 36,  164, 100, 228, 20,  148, 84,
            212, 52,  180, 116, 244, 12,  140, 76,  204, 44,  172, 108, 236, 28,  156, 92,  220, 60,  188, 124, 252, 2,   130, 66,  194, 34,  162, 98,  226, 18,  146, 82,  210, 50,  178, 114, 242, 10,  138, 74,  202, 42,  170,
            106, 234, 26,  154, 90,  218, 58,  186, 122, 250, 6,   134, 70,  198, 38,  166, 102, 230, 22,  150, 86,  214, 54,  182, 118, 246, 14,  142, 78,  206, 46,  174, 110, 238, 30,  158, 94,  222, 62,  190, 126, 254, 1,
            129, 65,  193, 33,  161, 97,  225, 17,  145, 81,  209, 49,  177, 113, 241, 9,   137, 73,  201, 41,  169, 105, 233, 25,  153, 89,  217, 57,  185, 121, 249, 5,   133, 69,  197, 37,  165, 101, 229, 21,  149, 85,  213,
            53,  181, 117, 245, 13,  141, 77,  205, 45,  173, 109, 237, 29,  157, 93,  221, 61,  189, 125, 253, 3,   131, 67,  195, 35,  163, 99,  227, 19,  147, 83,  211, 51,  179, 115, 243, 11,  139, 75,  203, 43,  171, 107,
            235, 27,  155, 91,  219, 59,  187, 123, 251, 7,   135, 71,  199, 39,  167, 103, 231, 23,  151, 87,  215, 55,  183, 119, 247, 15,  143, 79,  207, 47,  175, 111, 239, 31,  159, 95,  223, 63,  191, 127, 255,
        };

        struct Detail {
            uint8_t operator()(const uint8_t x) { return table[x]; }
        };

        template<typename FUNCTOR, typename T>
        constexpr T bitshuffle(const T x) {
            T result = 0;
            for (int i = 0; i < sizeof(T); ++i) {
                T const mask = T{0xFF} << (i * 8);
                T const tmp = FUNCTOR{}(static_cast<uint8_t>((x & mask) >> (i * 8)));
                result = (result << 8) | tmp;
            }
            return result;
        }

        template<typename FUNCTOR, typename T>
        constexpr T bitshuffleMemCopy(const T x) {
            std::array<uint8_t, sizeof(T)> input;
            std::array<uint8_t, sizeof(T)> output;
            std::memcpy(input.data(), &x, sizeof(T));

            for (std::size_t i = 0; i < sizeof(T); ++i) {
                output[sizeof(T) - 1 - i] = FUNCTOR{}(input[i]);
            }
            T result = 0;
            std::memcpy(&result, output.data(), sizeof(T));
            return result;
        }

        struct Wrapper {
            uint8_t operator()(const uint8_t x) const { return Detail{}(x); }

            uint16_t operator()(const uint16_t x) const { return bitshuffleMemCopy<Detail, uint16_t>(x); }

            uint32_t operator()(const uint32_t x) const { return bitshuffleMemCopy<Detail, uint32_t>(x); }

            uint64_t operator()(const uint64_t x) const { return bitshuffleMemCopy<Detail, uint64_t>(x); }

            int8_t operator()(const int8_t x) const { return static_cast<int8_t>(Detail{}(static_cast<uint8_t>(x))); }

            int16_t operator()(const int16_t x) const { return static_cast<int16_t>(bitshuffleMemCopy<Detail, uint16_t>(static_cast<uint16_t>(x))); }

            int32_t operator()(const int32_t x) const { return static_cast<int32_t>(bitshuffleMemCopy<Detail, uint32_t>(static_cast<uint32_t>(x))); }

            int64_t operator()(const int64_t x) const { return static_cast<int64_t>(bitshuffleMemCopy<Detail, uint64_t>(static_cast<uint64_t>(x))); }
        };

    }  // namespace bitshuffling


    template<IsDatatype U, bool invertBytes = true, bool flipBits = true, std::integral V>
    std::vector<std::bitset<U().bitwidth()>> toBitset(std::vector<V>& input) {
        if constexpr (flipBits) {
            bitshuffling::Wrapper wrap{};
            std::vector<int> test1(input.begin(), input.end());
            std::cout << join(test1, " ") << "\n";
            std::transform(input.cbegin(), input.cend(), input.begin(), wrap);
            std::vector<int> test2(input.begin(), input.end());
            std::cout << join(test2, " ") << "\n";
        }
        if constexpr (invertBytes) {
            std::vector<std::bitset<U().bitwidth()>> ret(input.begin(), input.end());
            return ret;
        } else {
            std::vector<std::bitset<U().bitwidth()>> ret(input.rbegin(), input.rend());
            return ret;
        }
    }

    template<IsDatatype U, bool flipBits = true, typename IteratorType>
    std::vector<std::bitset<U().bitwidth()>> toBitset(IteratorType first, IteratorType last) {
        using T = std::iterator_traits<IteratorType>::value_type;
        static_assert(std::is_integral<T>::value);
        if constexpr (flipBits) {
            std::vector<int> test1(first, last);
            std::cout << join(test1, " ") << "\n";
            bitshuffling::Wrapper wrap{};
            std::transform(first, last, first, wrap);
            std::vector<int> test2(first, last);
            std::cout << join(test1, " ") << "\n";
        }
        std::vector<std::bitset<U().bitwidth()>> ret(first, last);
        return ret;
    }

    template<IsDatatype U, bool invertDirection = true>
    finnBoost::dynamic_bitset<uint8_t> mergeBitsets(const std::vector<std::bitset<U().bitwidth()>>& input) {
        constexpr std::size_t bits = U().bitwidth();
        finnBoost::dynamic_bitset<uint8_t> ret;
        const std::size_t outputSize = input.size() * bits;
        ret.resize(outputSize);
        for (std::size_t i = 0; i < input.size(); ++i) {
            for (std::size_t j = 0; j < bits; ++j) {
                if constexpr (invertDirection) {
                    ret[outputSize - 1 - (i * bits + j)] = input[i][j];
                } else {
                    ret[i * bits + j] = input[i][j];
                }
            }
        }
        return ret;
    }

    template<typename T>
    std::vector<T> bitsetToByteVector(const finnBoost::dynamic_bitset<T>& input) {
        std::vector<uint8_t> ret;
        ret.reserve(input.num_blocks());
        finnBoost::to_block_range(input, std::back_inserter(ret));
        std::reverse(ret.begin(), ret.end());  // Maybe replace with stl::ranges::reverse?
        return ret;
    }

    template<IsDatatype U, typename IteratorType>
    std::vector<uint8_t> pack(IteratorType first, IteratorType last) {
        using T = std::iterator_traits<IteratorType>::value_type;
        if constexpr (std::endian::native == std::endian::big) {
            FinnUtils::logAndError<std::runtime_error>("Big-endian architectures are currently not supported!\n");
        } else if constexpr (std::endian::native == std::endian::little) {  // FINN Datatype is a byte long
            if constexpr (U().bitwidth() == 8) {                            // Input data is in uint8_t, then nop
                if constexpr (sizeof(T) == 1) {                             // Input data is in other one byte representation, then just cast
                    return std::vector<uint8_t>(first, last);
                } else {
                    FinnUtils::logAndError<std::runtime_error>("Not Implemented yet: 1 byte FinnType; !=1 Byte vector Type\n");
                }
            } else if constexpr ((U().bitwidth() % 8) == 0) {
                // Full bytes; Needs to be implemented.
            } else {
                auto bitsetvector = toBitset<U>(first, last);
                for (auto&& elem : bitsetvector) {
                    std::cout << elem.to_string() << "\n";
                }
                auto mergedBitset = mergeBitsets<U>(bitsetvector);
                std::string out;
                finnBoost::to_string(mergedBitset, out);
                std::cout << out << "\n";
                return bitsetToByteVector(mergedBitset);
            }
        } else {
            FinnUtils::logAndError<std::runtime_error>("Mixed architectures are currently not supported!\n");
        }
        return {};
    }

    template<IsDatatype U, typename T>
    std::vector<uint8_t> pack(std::vector<T>& foldedVec) {
        if constexpr (U().bitwidth() == 8 && std::is_same<T, uint8_t>::value) {
            return foldedVec;
        }
        return pack<U>(foldedVec.begin(), foldedVec.end());
    }

}  // namespace Finn

#endif  // DATAPACKING_HPP
