#ifndef FINN_UTILS_H
#define FINN_UTILS_H

#include <cstdint>
#include <numeric>
#include "Types.h"

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
    [[noreturn]] void logAndError(const std::string& msg) {
        FINN_LOG(Logger::getLogger(), loglevel::error) << msg;
        throw E(msg);
    }

    inline size_t shapeToElements(const shape_t& pShape) {
        return static_cast<size_t>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>()));
    }

    inline std::string shapeToString(const shape_t& pShape) {
        std::string str = "(";
        unsigned int index = 0;
        for (auto elem : pShape) {
            str.append(std::to_string(elem));
            if (index < pShape.size() - 2) {
                str.append(", ");
            }
            index++;
        }
        str.append(")");
        return str;
    }

}  // namespace FinnUtils

#endif
