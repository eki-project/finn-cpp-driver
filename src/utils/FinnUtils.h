#ifndef FINN_UTILS_H
#define FINN_UTILS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>

#include "Logger.h"
#include "Types.h"

namespace FinnUtils {

    /**
     * @brief Helper class to fill a vector with random values. Useful for testing the throughput?
     *
     */
    class BufferFiller {
         private:
        std::random_device rd;
        std::mt19937 engine{rd()};
        std::uniform_int_distribution<uint8_t> sampler;

         public:
        BufferFiller(uint8_t min, uint8_t max) : sampler(std::uniform_int_distribution<uint8_t>(min, max)) {}

        void fillRandom(std::vector<uint8_t>& vec) {
            std::transform(vec.begin(), vec.end(), vec.begin(), [this]([[maybe_unused]] uint8_t x) { return sampler(engine); });
        }
    };

    /**
     * @brief Return the ceil of a float as in integer type
     *
     * @param num
     * @return constexpr int32_t
     */
    constexpr int32_t ceil(float num) { return (static_cast<float>(static_cast<int32_t>(num)) == num) ? static_cast<int32_t>(num) : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0); }

    /**
     * @brief Return the innermost dimension of a shape. For example for (1,30,10) this would return 10
     *
     * @param shape
     * @return unsigned int
     */
    inline unsigned int innermostDimension(const shape_t& shape) { return shape.back(); }

    /**
     * @brief The XRT buffers have to be of a certain size and alignment. The size needs to be a power of 2, depending on the platform >=4096 and page-aligned. This returns the correct size
     *
     * @param requiredBytes The number of bytes that are needed. The return value will be greater or equal than this
     * @return unsigned int
     */
    inline size_t getActualBufferSize(size_t requiredBytes) { return static_cast<size_t>(std::max(4096.0, pow(2, log2(static_cast<double>(requiredBytes))))); }

    /**
     * @brief Get the number of elements required to represent S elements of FINN datatype F in C++ datatypes DT in the datatype T.
     * Example:
     * F = DatatypeUint<14>
     * T = uint8_t
     * DT = uint16_t
     * S = 10
     *
     * This would require 2 T per one F.
     *
     * @tparam T
     * @tparam F
     * @tparam DT
     * @tparam S
     * @return constexpr size_t
     */
    template<typename T, typename F, typename DT, size_t S>
    constexpr size_t getPackedElementSize() {
        return S * static_cast<size_t>(ceil(F().bitwidth() / (sizeof(T) * 8.0F)));
    }

    /**
     * @brief Returns the number of sizeof(T) iterations needed to scan all the data of the F value. This value may differ if sizeof(DT) is much larger than sizeof(F).
     * For example if the Finn datatype is DatatypeUint<4> but DT is a 256 bit uint, one would only need to scan one 8-bit sector of the 256 bit variable to get the necessary data
     *
     * @tparam T
     * @tparam F
     * @tparam DT
     * @return constexpr size_t
     */
    template<typename T, typename F, typename DT>
    constexpr size_t iterationsNeededPerDT() {
        return std::min(ceil(F().bitwidth() / sizeof(T) * 8.0F), ceil(static_cast<float>(sizeof(DT)) / static_cast<float>(sizeof(T))));  // Can leave out * 8 on both sides of division
    }

    /**
     * @brief Put some newlines into the log script for clearer reading 
     * 
     * @param logger 
     */
    inline void logSpacer(logger_type& logger) {
        FINN_LOG(logger, loglevel::info) << "\n\n\n\n";
    } 

    /**
     * @brief Log out the given number of results in a vector. If entriesToRead is larger than the vector size, the whole vector gets printed 
     * 
     * @param logger 
     * @param results 
     * @param entriesToRead 
     * @param prefix 
     */
    template<typename T>
    inline void logResults(logger_type& logger, const std::vector<T>& results, unsigned int entriesToRead, const std::string& prefix = "") {
        FINN_LOG(logger, loglevel::info) << prefix << "Values: ";
        std::string s = "";
    for (unsigned int i = 0; i < std::min(entriesToRead, static_cast<unsigned int>(results.size())); i++) {
            s += std::to_string(results[i]) + " ";
        }
        FINN_LOG(logger, loglevel::info) << s;
    }

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

    inline size_t shapeToElements(const shape_t& pShape) { return static_cast<size_t>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>())); }

    template<typename T, size_t S>
    constexpr size_t shapeToElementsConstexpr(std::array<T, S> pShape) {
        return static_cast<size_t>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>()));
    }

    /**
     * @brief Return a string representation of shape vector
     *
     * @param pShape
     * @return std::string
     */
    inline std::string shapeToString(const shape_t& pShape) {
        std::string str = "(";
        unsigned int index = 0;
        for (auto elem : pShape) {
            str.append(std::to_string(elem));
            if (index < pShape.size() - 1) {
                str.append(", ");
            }
            index++;
        }
        str.append(")");
        return str;
    }

    /**
     * @brief Implement std::unreachable
     *
     */
    [[noreturn]] inline void unreachable() {
        // Uses compiler specific extensions if possible.
        // Even if no extension is used, undefined behavior is still raised by
        // an empty function body and the noreturn attribute.
#ifdef __GNUC__  // GCC, Clang, ICC
        __builtin_unreachable();
#else
    #ifdef _MSC_VER  // MSVC
        __assume(false);
    #endif
#endif
    }

}  // namespace FinnUtils

#endif
