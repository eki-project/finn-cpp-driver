/**
 * @file AlignedAllocator.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements an allocator that allocates memory aligned to a configured
 * alignment. This allocator is compatible to the aligned allocators provided by
 * libraries such as Intel MKL and OpenBLAS.
 *
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 */

#ifndef ALIGNEDALLOCATOR_HPP
#define ALIGNEDALLOCATOR_HPP

#include <iostream>
#include <limits>

/**
 * @brief Allocator class for aligned allocs.This allocator is compatible to the
 * aligned allocators provided by libraries such as Intel MKL and OpenBLAS.
 * Methods are default methods needed to implement a costum allocator
 *
 * @tparam T type of which elements should be allocated
 * @tparam TALIGN alignment border
 * @tparam TBLOCK block size of allocated blocks
 */
template<typename T, size_t TALIGN = 64>
class AlignedAllocator {
     public:
    /**
     * @brief pointer type of Aligned Allocator
     *
     */
    using pointer = T*;
    /**
     * @brief Const pointer type of Aligned Allocator
     *
     */
    using const_pointer = const T*;
    /**
     * @brief Reference type of Aligned Allocator
     *
     */
    using reference = T&;
    /**
     * @brief Const reference type of Aligned Allocator
     *
     */
    using const_reference = const T&;
    /**
     * @brief Value type of Aligned Allocator
     *
     */
    using value_type = T;
    /**
     * @brief Size type of Aligned Allocator
     *
     */
    using size_type = std::size_t;
    /**
     * @brief ptrdiff type for Aligned Allocator
     *
     */
    using difference_type = ptrdiff_t;

    /**
     * @brief Returns address of r
     *
     * @param r
     * @return T*
     */
    inline T* address(T& r) const { return &r; }

    /**
     * @brief Returns address of const r
     *
     * @param s
     * @return const T*
     */
    static inline const T* address(const T& s) { return &s; }

    /**
     * @brief Returns the maximum size of allocated memory
     *
     * @return std::size_t
     */
    // NOLINTNEXTLINE
    inline std::size_t static max_size() { return std::numeric_limits<size_type>::max() / sizeof(T); }


    /**
     * @brief Internal Allocator stuff. Most likely will be replaced in the future.
     *
     * @tparam U
     */
    template<typename U>
    // NOLINTNEXTLINE
    struct rebind {
        /**
         * @brief Internal Allocator stuff. Most likely will be replaced in the future.
         *
         */
        using other = AlignedAllocator<U, TALIGN>;
    };

    /**
     * @brief Returns true if and only if storage allocated from *this can be deallocated from other, and vice versa. Always returns true for stateless allocators.
     *
     * @param other
     * @return true
     * @return false
     */
    inline bool operator==([[maybe_unused]] const AlignedAllocator& other) const { return true; }

    /**
     * @brief Not equal operator
     *
     * @param other
     * @return true
     * @return false
     */
    inline bool operator!=(const AlignedAllocator& other) const { return !(*this == other); }

    /**
     * @brief Construct a new Aligned Allocator object
     *
     */
    inline AlignedAllocator() = default;
    /**
     * @brief Destroy the Aligned Allocator object
     *
     */
    inline ~AlignedAllocator() = default;
    /**
     * @brief Construct a new Aligned Allocator object
     *
     */
    inline AlignedAllocator(const AlignedAllocator<T, TALIGN>&) noexcept = default;
    /**
     * @brief Construct a new Aligned Allocator object
     *
     */
    inline AlignedAllocator(AlignedAllocator<T, TALIGN>&&) noexcept = default;
    /**
     * @brief Move assignment operator
     *
     * @return AlignedAllocator<T, TALIGN>&
     */
    inline AlignedAllocator<T, TALIGN>& operator=(AlignedAllocator<T, TALIGN>&&) noexcept = default;
    /**
     * @brief Copy assignment operator
     *
     * @return AlignedAllocator<T, TALIGN>&
     */
    inline AlignedAllocator<T, TALIGN>& operator=(const AlignedAllocator<T, TALIGN>&) noexcept = default;

    /**
     * @brief Allocates n bytes aligned to a TALIGN alignment
     *
     * @param n number of bytes to allocate
     * @return T* pointer to allocated memory
     */
    [[nodiscard]] inline T* allocate(size_t n) {
        if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            std::cerr << "Tried to allocate too much at the same time. Overflow in "
                         "AlignedAllocator\n";
            throw std::bad_array_new_length();
        }

        T* ptr = NULL;
        size_t bytes = sizeof(T) * n;
        size_t allocBytes = ((bytes / TALIGN) + ((bytes % TALIGN != 0) ? 1 : 0)) * TALIGN;  // Only a multiple of TALIGN can be allocated.

        if ((ptr = static_cast<T*>(aligned_alloc(TALIGN, allocBytes)))) {
            return ptr;
        }


        throw std::bad_alloc();
    }

    /**
     * @brief Free allocated memory
     *
     * @param p Pointer to memory that should be freed
     * @param n Unused! Only for compatability
     */
    // NOLINTNEXTLINE
    inline void static deallocate(T* p, [[maybe_unused]] size_t n = 0) { std::free(p); }
};


#endif  // ALIGNEDALLOCATOR_HPP
