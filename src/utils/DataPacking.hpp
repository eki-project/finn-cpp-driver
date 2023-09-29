#ifndef DATAPACKING_HPP
#define DATAPACKING_HPP

#include <algorithm>
#include <bit>
#include <bitset>
// #include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <concepts>
#include <cstdint>
#include <vector>

#include "FinnDatatypes.hpp"
#include "FinnUtils.h"

namespace Finn {

    template<IsDatatype U, std::integral V>
    std::vector<std::bitset<U().bitwidth()>> toBitset(const std::vector<V>& input) {
        std::vector<std::bitset<U().bitwidth()>> ret(input.begin(), input.end());
        return ret;
    }

    // template<IsDatatype U>
    // FinnBoost::dynamic_bitset<> mergeBitsets(const std::vector<std::bitset<U().bitwidth()>>& input) {}

    template<IsDatatype U, typename T>
    std::vector<uint8_t> pack(const std::vector<T>& foldedVec) {
        if constexpr (std::endian::native == std::endian::big) {
            FinnUtils::logAndError<std::runtime_error>("Big-endian architectures are currently not supported!\n");
        } else if constexpr (std::endian::native == std::endian::little) {
            // Little Endian
            if constexpr (U().bitwidth() == 8) {                  // FINN Datatype is a byte long
                if constexpr (std::is_same<T, uint8_t>::value) {  // Input data is in uint8_t, then nop
                    return foldedVec;
                } else if (sizeof(T) == 1) {  // Input data is in other one byte representation, then just cast
                    return std::vector<uint8_t>(foldedVec.begin(), foldedVec.end());
                } else {
                }
            }
        } else {
            FinnUtils::logAndError<std::runtime_error>("Mixed architectures are currently not supported!\n");
        }
        return {};
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
                }
            }
        } else {
            FinnUtils::logAndError<std::runtime_error>("Mixed architectures are currently not supported!\n");
        }
        return {};
    }

}  // namespace Finn

#endif  // DATAPACKING_HPP
