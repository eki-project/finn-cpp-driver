#ifndef DATAPACKING_HPP
#define DATAPACKING_HPP

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

#include "FinnDatatypes.hpp"
#include "FinnUtils.h"

namespace Finn {

    template<typename T, IsDatatype U>
    std::vector<uint8_t> pack(const std::vector<T>& foldedVec) {
        if constexpr (std::endian::native == std::endian::big) {
            FinnUtils::logAndError<std::runtime_error>("Big-endian architectures are currently not supported!\n");
        } else if constexpr (std::endian::native == std::endian::little) {
            if constexpr (U().bitwidth() == 8 && std::is_same<T, uint8_t>::value) {
                return foldedVec;
            }
        } else {
            FinnUtils::logAndError<std::runtime_error>("Mixed architectures are currently not supported!\n");
        }
        return {};
    }

    template<typename T, IsDatatype U, typename IteratorType>
    std::vector<uint8_t> pack(IteratorType first, IteratorType last) {
        static_assert(std::is_same<typename std::iterator_traits<IteratorType>::value_type, T>::value);
        if constexpr (std::endian::native == std::endian::big) {
            FinnUtils::logAndError<std::runtime_error>("Big-endian architectures are currently not supported!\n");
        } else if constexpr (std::endian::native == std::endian::little) {
            if constexpr (std::is_same<T, uint8_t>::value) {
                return std::vector<uint8_t>(first, last);
            }
        } else {
            FinnUtils::logAndError<std::runtime_error>("Mixed architectures are currently not supported!\n");
        }
        return {};
    }

}  // namespace Finn

#endif  // DATAPACKING_HPP
