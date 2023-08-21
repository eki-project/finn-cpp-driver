#ifndef _UTILS_MDSPAN_H_
#define _UTILS_MDSPAN_H_

#include <span>

#ifdef __cpp_lib_mdspan
using stdex::mdspan = std::mdspan;

#else

    #include <mdspan/mdspan.hpp>
namespace stdex = Kokkos;

#endif

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
 * @return auto mdspan
 */
template<typename T, std::size_t N, typename Indices = std::make_index_sequence<N>>
auto makeMDSpan(T* data, const T (&list)[N]) {
    return makeMDSpanImpl(data, list, Indices{});
}

#endif  //_UTILS_MDSPAN_H_
