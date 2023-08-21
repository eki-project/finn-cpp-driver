#ifndef _UTILS_CREATEARRAYS_H_
#define _UTILS_CREATEARRAYS_H_

#include <array>

template<typename ArrayElements, typename... Tail>
static constexpr auto create_array(Tail&&... tail) {
    return std::array<ArrayElements, sizeof...(Tail)>{std::forward<Tail>(tail)...};
}

#endif  // _UTILS_CREATEARRAYS_H_