#ifndef COSTUMDYNAMICBITSET_H
#define COSTUMDYNAMICBITSET_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <execution>
#include <iterator>
#include <ranges>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utils/AlignedAllocator.hpp>
#include <vector>


class DynamicBitset {
     public:
    std::vector<uint8_t, AlignedAllocator<uint8_t>> bits;

     private:
    constexpr static std::size_t bitsPerByte = 8;
    size_t capacity;
    size_t bytes;

     public:
    DynamicBitset(const std::size_t& n) : bits(((n / bitsPerByte) + ((n % bitsPerByte != 0) ? 1 : 0)), 0), capacity(bits.size() * bitsPerByte), bytes(bits.size()) {}
    DynamicBitset(DynamicBitset&&) = default;
    DynamicBitset(const DynamicBitset&) = default;
    DynamicBitset& operator=(DynamicBitset&& other) {
        capacity = other.capacity;
        bytes = other.bytes;
        std::swap(this->bits, other.bits);
        return *this;
    }
    DynamicBitset& operator=(const DynamicBitset&) = delete;
    ~DynamicBitset() = default;

    std::size_t size() const { return capacity; }

    std::size_t numBytes() const { return bytes; }

    bool all() const {
        for (auto&& elem : bits) {
            if (elem != 255U) {
                return false;
            }
        }
        return true;
    }

    bool none() const {
        for (auto&& elem : bits) {
            if (elem != 0) {
                return false;
            }
        }
        return true;
    }

    void setSingleBit(std::size_t n) {
        std::size_t index = n / bitsPerByte;
        std::size_t bit = n % bitsPerByte;
        bits[index] |= static_cast<uint8_t>(1U << bit);
    }

    template<typename T>
    void setByte(const T x, std::size_t n) {
        static_assert(std::is_unsigned<T>::value, "DynamicBitset is only supported for unsigned types");
        std::array<uint8_t, sizeof(T)> input;
        const std::size_t bitOffset = n % bitsPerByte;
        const std::size_t byte = n / bitsPerByte;
        const std::size_t bitShift = sizeof(T) * bitsPerByte - bitOffset;
        if (bitOffset != 0) {
            // handle overflow nach links beim shiften
            const T bitMask = static_cast<T>(0xFF) << bitShift;
            const T overflow = (bitMask & x);
            if (overflow != 0) {
                auto overflowConverted = static_cast<uint8_t>(overflow >> bitShift);
                bits[byte + input.size()] |= overflowConverted;
            }
        }

        const T x2 = x << bitOffset;
        std::memcpy(input.data(), &x2, sizeof(T));

        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i] != 0) {
                bits[byte + i] |= input[i];
            }
        }
    }

    template<typename T>
    void outputBytes(std::back_insert_iterator<T> it) const {
        for (auto&& elem : bits) {
            it = elem;
        }
    }

    std::vector<uint8_t, AlignedAllocator<uint8_t>> getStorageVec() { return std::move(bits); }

    // NOLINTNEXTLINE
    std::string to_string() const {
        std::stringstream out;
        for (int i = static_cast<int>(bits.size()) - 1; i >= 0; --i) {
            for (std::size_t j = 0; j < 8; ++j) {
                out << (0 != (bits[static_cast<std::size_t>(i)] & (128U >> j)));
            }
        }
        return out.str();
    }

    /**
     * @brief Bitwise OR assignment operator. Should only be used in the context of OpenMP multithreading. Does not perform checks if the two Bitsets are of equal length!
     *
     * @param lhs first bitset (gets modified by operation)
     * @param rhs second bitset
     * @return DynamicBitset& merged bitset
     */
    friend DynamicBitset& operator|=(DynamicBitset& lhs, const DynamicBitset& rhs) {
        // TODO(linusjun): More testing
        std::transform(std::execution::par, lhs.bits.begin(), lhs.bits.end(), rhs.bits.begin(), lhs.bits.begin(), [](uint8_t& lhsByte, const uint8_t& rhsByte) { return lhsByte | rhsByte; });
        return lhs;
    }
};

#endif  // COSTUMDYNAMICBITSET_H
