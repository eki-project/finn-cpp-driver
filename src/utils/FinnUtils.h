#ifndef FINN_UTILS_H
#define FINN_UTILS_H

#include <cstdint>

namespace FinnUtils {
    constexpr int32_t ceil(float num) {
        return (static_cast<float>(static_cast<int32_t>(num)) == num)
            ? static_cast<int32_t>(num)
            : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
    }
}
#endif FINN_UTILS_H