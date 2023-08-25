#ifndef FINN_UTILS_H
#define FINN_UTILS_H

#include <cstdint>

#include "Logger.h"

namespace FinnUtils {

    /**
     * @brief Return the ceil of a float as in integer type
     *
     * @param num
     * @return constexpr int32_t
     */
    constexpr int32_t ceil(float num) { return (static_cast<float>(static_cast<int32_t>(num)) == num) ? static_cast<int32_t>(num) : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0); }

    /**
     * @brief First log the message as an error into the logger, then throw the passed error!
     *
     * @tparam E
     * @param msg
     */
    template<typename E>
    void logAndError(std::string msg) {
        FINN_LOG(Logger::getLogger(), loglevel::error) << msg;
        throw E(msg);
    }
}  // namespace FinnUtils

#endif
