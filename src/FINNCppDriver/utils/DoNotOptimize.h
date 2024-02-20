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
     * @brief Disables compiler optimization of unused variables for specific variable
     *
     * @tparam Tp
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<std::is_trivially_copyable<Tp>::value && (sizeof(Tp) <= sizeof(Tp*))>::type DoNotOptimize(Tp& value) {
        asm volatile("" : "+m,r"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable
     *
     * @tparam Tp
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<!std::is_trivially_copyable<Tp>::value || (sizeof(Tp) > sizeof(Tp*))>::type DoNotOptimize(Tp& value) {
        asm volatile("" : "+m"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable
     *
     * @tparam Tp
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<std::is_trivially_copyable<Tp>::value && (sizeof(Tp) <= sizeof(Tp*))>::type DoNotOptimize(Tp&& value) {
        asm volatile("" : "+m,r"(value) : : "memory");
    }

    /**
     * @brief Disables compiler optimization of unused variables for specific variable
     *
     * @tparam Tp
     */
    template<class Tp>
    inline __attribute__((always_inline)) typename std::enable_if<!std::is_trivially_copyable<Tp>::value || (sizeof(Tp) > sizeof(Tp*))>::type DoNotOptimize(Tp&& value) {
        asm volatile("" : "+m"(value) : : "memory");
    }
}  // namespace Finn


#endif  // DONOTOPTIMIZE
