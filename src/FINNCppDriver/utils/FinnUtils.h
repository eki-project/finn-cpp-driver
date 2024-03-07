/**
 * @file FinnUtils.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Provides various helper functions
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef FINN_UTILS_H
#define FINN_UTILS_H

#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>

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
        /**
         * @brief Construct a new Buffer Filler object
         *
         * @param min
         * @param max
         */
        BufferFiller(uint8_t min, uint8_t max) : sampler(std::uniform_int_distribution<uint8_t>(min, max)) {}

        /**
         * @brief
         *
         * @param min
         * @param max
         * @return BufferFiller
         */
        static BufferFiller create(uint8_t min, uint8_t max) { return {min, max}; }

        /**
         * @brief
         *
         * @tparam IteratorType
         * @param first
         * @param last
         */
        template<typename IteratorType>
        void fillRandom(IteratorType first, IteratorType last) {
            std::generate(first, last, [this, &sampler = sampler, &engine = engine]() { return sampler(engine); });
        }

        /**
         * @brief
         *
         * @param vec
         */
        void fillRandom(std::vector<uint8_t>& vec) { fillRandom(vec.begin(), vec.end()); }
    };


    /**
     * @brief Implements FloatingPoint concept
     *
     * @tparam T
     */
    template<typename T>
    concept FloatingPoint = std::is_floating_point_v<T> && (sizeof(T) == 4 || sizeof(T) == 8) &&  // Only 32/64 bit allowed. 80 bit fp not allowed
                            sizeof(float) == 4 && sizeof(double) == 8 &&                          // float must be 32 bit fp while double must be 64 bit fp
                            std::numeric_limits<T>::is_iec559 &&                                  // Only IEEE 754  fp allowed
                            std::endian::native == std::endian::little;

    /**
     * @brief Helper function for ceil. Checks if param is inf. Based on https://codereview.stackexchange.com/questions/248169/two-constexpr-ceil-functions
     *
     * @tparam T
     * @param inFp
     * @return true
     * @return false
     */
    template<FloatingPoint T>
    constexpr bool isInf(T inFp)  // detect if infinity or -infinity
    {
        constexpr bool isTFloat = std::is_same_v<T, float>;
        using uintN_t = std::conditional_t<isTFloat, uint32_t, uint64_t>;
        using intN_t = std::conditional_t<isTFloat, int32_t, int64_t>;

        constexpr uintN_t mantissaBitNumber = isTFloat ? 23 : 52;
        constexpr uintN_t infinityExponentValue = isTFloat ? 0xff : 0x7ff;                     // the value of the exponent if infinity
        constexpr uintN_t positiveInfinityValue = infinityExponentValue << mantissaBitNumber;  // the value of positive infinity
        constexpr uintN_t signRemovalMask = std::numeric_limits<intN_t>::max();                // the max value of a signed int is all bits set to one except sign

        return ((std::bit_cast<uintN_t, T>(inFp) & signRemovalMask) == positiveInfinityValue);  // remove sign before comparing against positive infinity value
    }

    /**
     * @brief Helper function for ceil. Checks if param is NaN. Based on https://codereview.stackexchange.com/questions/248169/two-constexpr-ceil-functions
     *
     * @tparam T
     * @param inFp
     * @return true
     * @return false
     */
    template<FloatingPoint T>
    constexpr bool isNaN(T inFp) {
        constexpr bool isTFloat = std::is_same_v<T, float>;
        using uintN_t = std::conditional_t<isTFloat, uint32_t, uint64_t>;
        using intN_t = std::conditional_t<isTFloat, int32_t, int64_t>;

        constexpr uintN_t mantissaBitNumber = isTFloat ? 23 : 52;
        constexpr uintN_t naNExponentValue = isTFloat ? 0xff : 0x7ff;            // the value of the exponent if NaN
        constexpr uintN_t signRemovalMask = std::numeric_limits<intN_t>::max();  // the max value of a signed int is all bits set to one except sign
        constexpr uintN_t exponentMask = naNExponentValue << mantissaBitNumber;
        constexpr uintN_t mantissaMask = (~exponentMask) & signRemovalMask;  // the bits of the mantissa are 1's, sign and exponent 0's.

        return (((std::bit_cast<uintN_t, T>(inFp) & exponentMask) == exponentMask) &&  // if exponent is all 1's
                ((std::bit_cast<uintN_t, T>(inFp) & mantissaMask) != 0)                // if mantissa is != 0
        );
    }

    /**
     * @brief constexpr ceil function based on https://codereview.stackexchange.com/questions/248169/two-constexpr-ceil-functions
     *
     * @tparam T
     * @param inFp
     * @return constexpr T
     */
    template<FloatingPoint T>
    constexpr T ceil(const T inFp) {
        if (isInf<T>(inFp) || isNaN<T>(inFp)) {
            return inFp;
        }

        constexpr bool isTFloat = std::is_same_v<T, float>;
        using intN_t = std::conditional_t<isTFloat, int32_t, int64_t>;

        // NOLINTBEGIN
        if (inFp > 0 && inFp != static_cast<intN_t>(inFp)) {  // These lossy conversions are intended for rounding
            return static_cast<intN_t>(inFp + 1);             // These lossy conversions are intended for rounding
        } else {
            return static_cast<intN_t>(inFp);  // These lossy conversions are intended for rounding
        }
        // NOLINTEND
    }

    const std::array<int,64> tab64 = {
        63,  0, 58,  1, 59, 47, 53,  2,
        60, 39, 48, 27, 54, 33, 42,  3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22,  4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16,  9, 12,
        44, 24, 15,  8, 23,  7,  6,  5};

    inline std::size_t log2_64 (std::size_t value)
    {
        static_assert(sizeof(std::size_t)==8);
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;
        return static_cast<std::size_t>(tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58]);
    }

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
    inline size_t getActualBufferSize(size_t requiredBytes) { return static_cast<size_t>(std::max(4096.0, pow(2, ceil(log2(static_cast<double>(requiredBytes)))))); }

    /**
     * @brief Get the number of elements required to represent S elements of FINN datatype F in the datatype T.
     *
     * @tparam T
     * @tparam F
     * @tparam S
     * @return constexpr size_t
     */
    template<typename T = uint8_t, typename F, size_t S>
    constexpr size_t getPackedElementSize() {
        return static_cast<size_t>(ceil(S * (F().bitwidth() / (sizeof(T) * 8.0F))));
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
    inline void logSpacer(logger_type& logger) { FINN_LOG(logger, loglevel::info) << "\n\n\n\n"; }

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
        std::string str;
        for (unsigned int i = 0; i < std::min(entriesToRead, static_cast<unsigned int>(results.size())); i++) {
            str += std::to_string(results[i]) + " ";
        }
        FINN_LOG(logger, loglevel::info) << str;
    }


    /**
     * @brief Calculates the number of elements in a tensor given its shape.
     * @attention This does NOT calculate the size of a buffer on that same tensor. Due to XRT min page size, every size is atleast 4096 elements large!!!
     *
     * @param pShape
     * @return size_t
     */
    inline size_t shapeToElements(const shape_t& pShape) { return (pShape.empty()) ? 0 : static_cast<size_t>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>())); }

    /**
     * @brief Constexpr version of shapeToElements
     *
     * @tparam T
     * @tparam S
     * @param pShape
     * @return constexpr size_t
     */
    template<typename T, size_t S>
    constexpr size_t shapeToElementsConstexpr(std::array<T, S> pShape) {
        return (S == 0) ? 0 : static_cast<size_t>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>()));
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

}  // namespace FinnUtils

#endif
