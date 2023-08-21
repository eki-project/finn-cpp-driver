#ifndef _UTILS_MDSPAN_H_
#define _UTILS_MDSPAN_H_

#include <span>

#ifdef __cpp_lib_mdspan
using stdex::mdspan = std::mdspan;

#else

    #include <mdspan/mdspan.hpp>
namespace stdex = Kokkos;

#endif

template<typename T, typename Array, std::size_t... I>
auto makeMDSpanImpl(T* data, const Array& a, [[maybe_unused]] std::index_sequence<I...> u) {
    return stdex::mdspan(data, a[I]...);
}

template<typename T, std::size_t N, typename Indices = std::make_index_sequence<N>>
auto makeMDSpan(T* data, const T (&list)[N]) {
    return makeMDSpanImpl(data, list, Indices{});
}

#endif  //_UTILS_MDSPAN_H_
