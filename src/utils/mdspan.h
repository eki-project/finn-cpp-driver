#ifndef _UTILS_MDSPAN_H_
#define _UTILS_MDSPAN_H_

#ifdef __cpp_lib_mdspan
using stdex::mdspan = std::mdspan;

#else

  #include <mdspan/mdspan.hpp>
namespace stdex = Kokkos;

#endif

#endif  //_UTILS_MDSPAN_H_
