#ifndef _UTILS_MDSPAN_H_
#define _UTILS_MDSPAN_H_

#ifdef __cpp_lib_mdspan
using stdex::mdspan = std::mdspan;

#else

  #include <mdspan/mdspan.hpp>
namespace stdex = Kokkos;

#endif

template<typename Array, std::size_t... I>
auto makeMDSpanImpl(int* data, const Array& a, std::index_sequence<I...>) {
  return stdex::mdspan(data, a[I]...);
}

template<typename T, std::size_t N, typename Indices = std::make_index_sequence<N>>
auto makeMDSpan(T* data, const T (&list)[N]) {
  return makeMDSpanImpl(data, list, Indices{});
}

#endif  //_UTILS_MDSPAN_H_
