/**
 * @file mdspan.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides the C++23 functionality of mdspan
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef MDSPAN_H
#define MDSPAN_H

#include <span>

#ifdef __cpp_lib_mdspan
    #warning("Warning: Both std and stdex mdspan are enabled. stdex should be disabled as it is deprecated! ")
#endif


#include <mdspan/mdspan.hpp>
namespace stdex = Kokkos;


/**
 * @brief Implementation of makeMDSpan
 *
 * @tparam T Type of data stored in array
 * @tparam Array Type of array of dimensions
 * @tparam I Internal
 * @param data Pointer to underlying data array
 * @param a array of dimensions
 * @param u index sequence (unused)
 * @return auto mdspan
 */
template<typename T, typename Array, std::size_t... I>
auto makeMDSpanImpl(T* data, const Array& a, [[maybe_unused]] std::index_sequence<I...> u) {
    return stdex::mdspan(data, a[I]...);
}

/**
 * @brief Constructs a mdspan
 *
 * @tparam T Type of data stored in array
 * @tparam N Number of dimensions (autodeduced)
 * @tparam Indices Index sequence for dimension array (autodeduced)
 * @param data Pointer to underlying data array
 * @param list
 * @return auto mdspan
 */
template<typename T, std::size_t N, typename Indices = std::make_index_sequence<N>>
// NOLINTNEXTLINE
auto makeMDSpan(T* data, const T (&list)[N]) {
    return makeMDSpanImpl(data, list, Indices{});
}

#endif  // MDSPAN_H
