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

/**
 * @brief A storage class to store a dynamic amount of bits. Assumes that each bit is set to 1 at most once. Bits cannot be reset.
 *
 */
class DynamicBitset {
     public:
    std::vector<uint8_t, AlignedAllocator<uint8_t>> bits;

     private:
    constexpr static std::size_t bitsPerByte = 8;
    size_t capacity;
    size_t bytes;

     public:
    /**
     * @brief Construct a new Dynamic Bitset
     *
     * @param n Number of bits that should be stored
     */
    DynamicBitset(const std::size_t& n) : bits(((n / bitsPerByte) + ((n % bitsPerByte != 0) ? 1 : 0)), 0), capacity(bits.size() * bitsPerByte), bytes(bits.size()) {}
    /**
     * @brief Move constructor
     *
     */
    DynamicBitset(DynamicBitset&&) = default;
    /**
     * @brief Copy constructor
     *
     */
    DynamicBitset(const DynamicBitset&) = default;
    /**
     * @brief Move assignment
     *
     * @param other
     * @return DynamicBitset&
     */
    DynamicBitset& operator=(DynamicBitset&& other) {
        capacity = other.capacity;
        bytes = other.bytes;
        std::swap(this->bits, other.bits);
        return *this;
    }

    /**
     * @brief Copy assignment
     *
     * @return DynamicBitset&
     */
    DynamicBitset& operator=(const DynamicBitset&) = delete;

    /**
     * @brief Destroy the Dynamic Bitset object
     *
     */
    ~DynamicBitset() = default;

    /**
     * @brief Returns the capacity in bits of the bitset
     *
     * @return std::size_t
     */
    std::size_t size() const { return capacity; }

    /**
     * @brief Returns the number of bytes stored in the bitset
     *
     * @return std::size_t
     */
    std::size_t numBytes() const { return bytes; }

    /**
     * @brief Tests if all of the bits contained in the dynamic bitset is set.
     *
     * @return true
     * @return false
     */
    bool all() const {
        for (auto&& elem : bits) {
            if (elem != 255U) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Tests if none of the bits contained in the dynamic bitset is set.
     *
     * @return true
     * @return false
     */
    bool none() const {
        for (auto&& elem : bits) {
            if (elem != 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Sets a single bit. Cannot overwrite already set bits.
     *
     * @param n
     */
    void setSingleBit(std::size_t n) {
        std::size_t index = n / bitsPerByte;
        std::size_t bit = n % bitsPerByte;
        bits[index] |= static_cast<uint8_t>(1U << bit);
    }

    /**
     * @brief Sets multiple bytes at once based on the provided input. Cannot overwrite already set bits.
     *
     * @tparam T
     * @param x
     * @param n
     */
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

    /**
     * @brief Copies the internal storage vector to the provided container
     *
     * @tparam T
     * @param it
     */
    template<typename T>
    void outputBytes(std::back_insert_iterator<T> it) const {
        for (auto&& elem : bits) {
            it = elem;
        }
    }

    /**
     * @brief Moves the internal storage vector. The original Dynamic Bitset can not be used after this call!
     *
     * @return std::vector<uint8_t, AlignedAllocator<uint8_t>>
     */
    std::vector<uint8_t, AlignedAllocator<uint8_t>> getStorageVec() { return std::move(bits); }

    /**
     * @brief Converts a DynamicBitset to a string representation
     *
     * @return std::string
     */
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
        std::transform(lhs.bits.begin(), lhs.bits.end(), rhs.bits.begin(), lhs.bits.begin(), [](uint8_t& lhsByte, const uint8_t& rhsByte) { return lhsByte | rhsByte; });
        return lhs;
    }
};

#endif  // COSTUMDYNAMICBITSET_H
