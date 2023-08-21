#ifndef _UTILS_CREATEARRAYS_H_
#define _UTILS_CREATEARRAYS_H_

#include <array>

/**
 * @brief Create a std::array object without knowing its length
 *
 * @tparam ArrayElements Type of Array Content
 * @tparam Tail Autodeduced by template, internal stuff
 * @param tail Comma seperated parameter pack of array content
 * @return constexpr auto Created array
 */
template<typename ArrayElements, typename... Tail>
static constexpr auto createArray(Tail&&... tail) {
    return std::array<ArrayElements, sizeof...(Tail)>{std::forward<Tail>(tail)...};
}

#endif  // _UTILS_CREATEARRAYS_H_