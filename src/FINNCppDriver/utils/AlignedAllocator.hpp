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


#include <bit>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

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
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = ptrdiff_t;

    inline T* address(T& r) const { return &r; }

    static inline const T* address(const T& s) { return &s; }

    // NOLINTNEXTLINE
    inline std::size_t static max_size() { return std::numeric_limits<size_type>::max() / sizeof(T); }

    template<typename U>
    // NOLINTNEXTLINE
    struct rebind {
        using other = AlignedAllocator<U, TALIGN>;
    };

    // inline void static construct(T* const p, const T& t) {
    //     void* const ptrv = static_cast<void*>(p);

    //     new (ptrv) T(t);
    // }

    // inline void static destroy(T* const p) { p->~T(); }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    inline bool operator==([[maybe_unused]] const AlignedAllocator& other) const { return true; }

    inline bool operator!=(const AlignedAllocator& other) const { return !(*this == other); }

    inline AlignedAllocator() = default;
    inline ~AlignedAllocator() = default;
    inline AlignedAllocator(const AlignedAllocator<T, TALIGN>&) noexcept = default;
    inline AlignedAllocator(AlignedAllocator<T, TALIGN>&&) noexcept = default;
    inline AlignedAllocator<T, TALIGN>& operator=(AlignedAllocator<T, TALIGN>&&) noexcept = default;
    inline AlignedAllocator<T, TALIGN>& operator=(const AlignedAllocator<T, TALIGN>&) noexcept = default;

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

    // NOLINTNEXTLINE
    inline void static deallocate(T* p, [[maybe_unused]] size_t n = 0) { std::free(p); }
};


#endif  // ALIGNEDALLOCATOR_HPP
