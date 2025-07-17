/**
 * @file DoNotOptimize.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides functions to disable compiler optimization for unused variables
 * @version 0.1
 * @date 2023-02-10
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DONOTOPTIMIZE
#define DONOTOPTIMIZE

#include <type_traits>

// This Code is based on the Google benchmark library.

namespace Finn {

    /**
     * @brief Disables compiler optimization of unused variables for specific variable (trivially copyable types <= pointer size)
     *
     * @tparam Tp Type of the value (must be trivially copyable and size <= pointer size)
     * @param value Reference to the value to prevent optimization
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<std::is_trivially_copyable<Tp>::value && (sizeof(Tp) <= sizeof(Tp*))>::type DoNotOptimize(Tp& value) {
        asm volatile("" : "+m,r"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable (non-trivially copyable or large types)
     *
     * @tparam Tp Type of the value (non-trivially copyable or size > pointer size)
     * @param value Reference to the value to prevent optimization
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<!std::is_trivially_copyable<Tp>::value || (sizeof(Tp) > sizeof(Tp*))>::type DoNotOptimize(Tp& value) {
        asm volatile("" : "+m"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable (rvalue, trivially copyable <= pointer size)
     *
     * @tparam Tp Type of the value (must be trivially copyable and size <= pointer size)
     * @param value Rvalue reference to the value to prevent optimization
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<std::is_trivially_copyable<Tp>::value && (sizeof(Tp) <= sizeof(Tp*))>::type DoNotOptimize(Tp&& value) {
        asm volatile("" : "+m,r"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable (rvalue, non-trivially copyable or large types)
     *
     * @tparam Tp Type of the value (non-trivially copyable or size > pointer size)
     * @param value Rvalue reference to the value to prevent optimization
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<!std::is_trivially_copyable<Tp>::value || (sizeof(Tp) > sizeof(Tp*))>::type DoNotOptimize(Tp&& value) {
        asm volatile("" : "+m"(value) : : "memory");
    }
}  // namespace Finn


#endif  // DONOTOPTIMIZE
