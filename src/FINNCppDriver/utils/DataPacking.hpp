/**
 * @file DataPacking.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements data packing and unpacking efficiently
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DATAPACKING
#define DATAPACKING

#include <FINNCppDriver/utils/CustomDynamicBitset.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/AlignedAllocator.hpp>
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <algorithm>
#include <bitset>
#include <concepts>
#include <cstdint>
#include <iterator>

#include <omp.h>

namespace Finn {
    /**
     * @brief Namespace to extremely efficiently and fast flip the bits in a number.
     *
     */
    namespace bitshuffling {
        /**
         * @brief Lookup table for reversing a single byte
         */
        constexpr std::array<uint8_t, 256> table{
            0,   128, 64,  192, 32,  160, 96,  224, 16,  144, 80,  208, 48,  176, 112, 240, 8,   136, 72,  200, 40,  168, 104, 232, 24,  152, 88,  216, 56,  184, 120, 248, 4,   132, 68,  196, 36,  164, 100, 228, 20,  148, 84,
            212, 52,  180, 116, 244, 12,  140, 76,  204, 44,  172, 108, 236, 28,  156, 92,  220, 60,  188, 124, 252, 2,   130, 66,  194, 34,  162, 98,  226, 18,  146, 82,  210, 50,  178, 114, 242, 10,  138, 74,  202, 42,  170,
            106, 234, 26,  154, 90,  218, 58,  186, 122, 250, 6,   134, 70,  198, 38,  166, 102, 230, 22,  150, 86,  214, 54,  182, 118, 246, 14,  142, 78,  206, 46,  174, 110, 238, 30,  158, 94,  222, 62,  190, 126, 254, 1,
            129, 65,  193, 33,  161, 97,  225, 17,  145, 81,  209, 49,  177, 113, 241, 9,   137, 73,  201, 41,  169, 105, 233, 25,  153, 89,  217, 57,  185, 121, 249, 5,   133, 69,  197, 37,  165, 101, 229, 21,  149, 85,  213,
            53,  181, 117, 245, 13,  141, 77,  205, 45,  173, 109, 237, 29,  157, 93,  221, 61,  189, 125, 253, 3,   131, 67,  195, 35,  163, 99,  227, 19,  147, 83,  211, 51,  179, 115, 243, 11,  139, 75,  203, 43,  171, 107,
            235, 27,  155, 91,  219, 59,  187, 123, 251, 7,   135, 71,  199, 39,  167, 103, 231, 23,  151, 87,  215, 55,  183, 119, 247, 15,  143, 79,  207, 47,  175, 111, 239, 31,  159, 95,  223, 63,  191, 127, 255,
        };

        /**
         * @brief Functor used for byte reversing
         *
         */
        struct Detail {
            /**
             * @brief Functor used for byte reversing
             *
             * @param x Input that should be reversed
             * @return uint8_t Reversed output
             */
            uint8_t operator()(const uint8_t x) { return table[x]; }
        };

        /**
         * @brief Trivial implementation to reverse a n byte long type
         *
         * @tparam FUNCTOR Functor used for reversing single byte (usually functor using lookup table)
         * @tparam T Type of number to be reversed
         * @param x data to be reversed
         * @return constexpr T reversed version of x. Can be used at compiletime.
         */
        template<typename FUNCTOR, typename T>
        constexpr T bitshuffle(const T x) {
            static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
            T result = 0;
            for (int i = 0; i < sizeof(T); ++i) {
                T const mask = T{ 0xFF } << (i * 8);
                T const tmp = FUNCTOR{}(static_cast<uint8_t>((x & mask) >> (i * 8)));
                result = (result << 8) | tmp;
            }
            return result;
        }

        /**
         * @brief Optimized implementation to reverse a n byte long type using memcpy
         *
         * @tparam FUNCTOR Functor used for reversing single byte (usually functor using lookup table)
         * @tparam T Type of number to be reversed
         * @param x data to be reversed
         * @return constexpr T reversed version of x. Can be used at compiletime.
         */
        template<typename FUNCTOR, typename T>
        constexpr T bitshuffleMemCopy(const T x) {
            static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
            std::array<uint8_t, sizeof(T)> input;
            std::array<uint8_t, sizeof(T)> output;
            std::memcpy(input.data(), &x, sizeof(T));

            for (std::size_t i = 0; i < sizeof(T); ++i) {
                output[sizeof(T) - 1 - i] = FUNCTOR{}(input[i]);
            }
            T result = 0;
            std::memcpy(&result, output.data(), sizeof(T));
            return result;
        }

        /**
         * @brief Wrapper functor that provides call operators for all supported types
         *
         */
        struct Wrapper {
            /**
             * @brief Reverses 1 byte long input
             *
             * @param x
             * @return uint8_t
             */
            uint8_t operator()(const uint8_t x) const { return Detail{}(x); }

            /**
             * @brief Reverses 2 byte long input
             *
             * @param x
             * @return uint16_t
             */
            uint16_t operator()(const uint16_t x) const { return bitshuffleMemCopy<Detail, uint16_t>(x); }

            /**
             * @brief Reverses 4 byte long input
             *
             * @param x
             * @return uint32_t
             */
            uint32_t operator()(const uint32_t x) const { return bitshuffleMemCopy<Detail, uint32_t>(x); }

            /**
             * @brief Reverses 8 byte long input
             *
             * @param x
             * @return uint64_t
             */
            uint64_t operator()(const uint64_t x) const { return bitshuffleMemCopy<Detail, uint64_t>(x); }

            /**
             * @brief Reverses 1 byte long input
             *
             * @param x
             * @return int8_t
             */
            int8_t operator()(const int8_t x) const { return static_cast<int8_t>(Detail{}(static_cast<uint8_t>(x))); }

            /**
             * @brief Reverses 2 byte long input
             *
             * @param x
             * @return int16_t
             */
            int16_t operator()(const int16_t x) const { return static_cast<int16_t>(bitshuffleMemCopy<Detail, uint16_t>(static_cast<uint16_t>(x))); }

            /**
             * @brief Reverses 4 byte long input
             *
             * @param x
             * @return int32_t
             */
            int32_t operator()(const int32_t x) const { return static_cast<int32_t>(bitshuffleMemCopy<Detail, uint32_t>(static_cast<uint32_t>(x))); }

            /**
             * @brief Reverses 8 byte long input
             *
             * @param x
             * @return int64_t
             */
            int64_t operator()(const int64_t x) const { return static_cast<int64_t>(bitshuffleMemCopy<Detail, uint64_t>(static_cast<uint64_t>(x))); }
        };

    }  // namespace bitshuffling

    /**
     * @brief Namespace to autodeduce return type based on Finn Datatype
     *
     */
    namespace UnpackingAutoRetType {
        /**
         * @brief Returns a signed integer type with 4 bytes if input U can be stored in 4 bytes and a 8 byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using FourBytesOrLongerSigned = typename std::conditional<U().bitwidth() <= 32, int32_t, int64_t>::type;

        /**
         * @brief Returns a signed integer type with 2 bytes if input U can be stored in 2 bytes and a 4+ byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using TwoBytesOrLongerSigned = typename std::conditional<U().bitwidth() <= 16, int16_t, FourBytesOrLongerSigned<U>>::type;

        /**
         * @brief Returns a signed integer type with 1 bytes if input U can be stored in 1 bytes and a 2+ byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using SignedRetType = typename std::conditional<U().bitwidth() <= 8, int8_t, TwoBytesOrLongerSigned<U>>::type;

        /**
         * @brief Returns a unsigned integer type with 4 bytes if input U can be stored in 4 bytes and a 8 byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using FourBytesOrLongerUnsigned = typename std::conditional<U().bitwidth() <= 32, uint32_t, uint64_t>::type;

        /**
         * @brief Returns a unsigned integer type with 2 bytes if input U can be stored in 2 bytes and a 4 byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using TwoBytesOrLongerUnsigned = typename std::conditional<U().bitwidth() <= 16, uint16_t, FourBytesOrLongerUnsigned<U>>::type;

        /**
         * @brief Returns a unsigned integer type with 1 bytes if input U can be stored in 1 bytes and a 2 byte long type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using UnsignedRetType = typename std::conditional<U().bitwidth() <= 8, uint8_t, TwoBytesOrLongerUnsigned<U>>::type;

        /**
         * @brief Returns a unsigned integer type if input U unsigned and a signed type otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using IntegralType = typename std::conditional<U().sign(), SignedRetType<U>, UnsignedRetType<U>>::type;

        /**
         * @brief Returns a integer type if input U integer type and float otherwise
         *
         * @tparam U
         */
        template<IsDatatype U>
        using AutoRetType = typename std::conditional<U().isInteger(), IntegralType<U>, float>::type;
    }  // namespace UnpackingAutoRetType

    /**
     * @brief Creates a bitmask for the upper bits
     *
     * @tparam T Datatype of mask
     * @param bits Upper bits to be masked
     * @return consteval Not available at runtime
     */
    template<typename T>
    consteval T createMask(std::size_t bits) {
        T mask = 0;
        for (std::size_t i = 0; i < bits; ++i) {
            mask = mask | static_cast<T>((T(1U) << i));
        }
        return mask;
    }

    /**
     * @brief Internal implementations. Should not be used by user.
     *
     */
    namespace detail {
        /**
         * @brief Internal Implementation of toBitset
         *
         * @tparam U Finn Datatype of input data
         * @tparam invertBytes Flag to reverse direction of bytes in output
         * @tparam reverseBits Flag to reverse bit direction
         * @tparam IteratorType
         * @param first iterator pointing to first element of input
         * @param last iterator pointing to last element of input
         * @return Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> vector of bitsets containing bit representations of inputs
         */
        template<IsDatatype U, bool invertBytes = true, bool reverseBits = true, typename IteratorType>
        Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> toBitsetImpl(IteratorType first, IteratorType last) {
            using T = typename std::iterator_traits<IteratorType>::value_type;
            if constexpr (reverseBits) {
                constexpr std::size_t shift = (sizeof(T) * 8 - U().bitwidth());
                bitshuffling::Wrapper wrap{};
                std::transform(first, last, first, wrap);
                if constexpr (std::is_same_v<U, DatatypeBipolar>) {
                    std::transform(first, last, first, [](const T& val) { return (val + 1) >> (shift - 1); });  // This converts bipolar to binary
                }
                else {
                    std::transform(first, last, first, [](const T& val) { return val >> shift; });
                }
            }
            else {
                if constexpr (std::is_same_v<U, DatatypeBipolar>) {
                    std::transform(first, last, first, [](const T& val) { return (val + 1) >> 1; });  // This converts bipolar to binary
                }
            }
            constexpr T mask = createMask<T>(U().bitwidth());
            std::transform(first, last, first, [](const T& val) { return val & mask; });  // Cut away all bits larger than U().bitwidth()
            if constexpr (invertBytes) {
                Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> ret(first, last);
                return ret;
            }
            else {
                Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> ret(std::make_reverse_iterator(last), std::make_reverse_iterator(first));
                return ret;
            }
        }
    }  // namespace detail


    /**
     * @brief Converts vector of inputs to a vector of bitsets representing the inputs
     *
     * @tparam U Finn Datatype of input data
     * @tparam invertBytes Flag to reverse direction of bytes in output
     * @tparam reverseBits Flag to reverse bit direction
     * @tparam IteratorType
     * @param first iterator pointing to first element of input
     * @param last iterator pointing to last element of input
     * @return Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> vector of bitsets containing bit representations of inputs
     */
    template<IsDatatype U, bool invertBytes = true, bool reverseBits = true, typename IteratorType, typename = std::enable_if_t<std::is_integral<typename std::iterator_traits<IteratorType>::value_type>::value>>
    Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> toBitset(IteratorType first, IteratorType last) {
        using T = typename std::iterator_traits<IteratorType>::value_type;
        static_assert(sizeof(T) <= 8, "Datatypes with more than 8 bytes are currently not supported!");
        if constexpr (std::is_signed_v<T>) {  // Needs to be casted to an unsigned input type to avoid undefined behavior of shift operations on negative values
            using FourBytesOrLonger = typename std::conditional<sizeof(T) <= 4, uint32_t, uint64_t>::type;
            using TwoBytesOrLonger = typename std::conditional<sizeof(T) == 2, uint16_t, FourBytesOrLonger>::type;
            using OneByteOrLonger = typename std::conditional<sizeof(T) == 1, uint8_t, TwoBytesOrLonger>::type;

            Finn::vector<OneByteOrLonger> vec(first, last);
            return detail::toBitsetImpl<U, invertBytes, reverseBits>(vec.begin(), vec.end());

        }
        else {
            return detail::toBitsetImpl<U, invertBytes, reverseBits>(first, last);
        }
    }

    /**
     * @brief Converts vector of inputs to a vector of bitsets representing the inputs
     *
     * @tparam U Finn Datatype of input data
     * @tparam invertBytes Flag to reverse direction of bytes in output
     * @tparam reverseBits Flag to reverse bit direction
     * @tparam V
     * @param input Vector to be converted to vector of bitset
     * @return Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> vector of bitsets containing bit representations of inputs
     */
    template<IsDatatype U, bool invertBytes = true, bool reverseBits = true, std::integral V>
    Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> toBitset(Finn::vector<V>& input) {
        return toBitset<U, invertBytes, reverseBits, typename Finn::vector<V>::iterator>(input.begin(), input.end());
    }

    /**
     * @brief OpenMp OR reduction for DynamicBitsets
     *
     * @param inout Input and output dataset
     * @param in Input dataset
     */
    void bitsetOR(DynamicBitset& inout, DynamicBitset& in) { inout |= in; }
#pragma omp declare reduction(bitsetOR:DynamicBitset : bitsetOR(omp_out, omp_in)) initializer(omp_priv = omp_orig)


    /**
     * @brief Merges a vector of bitsets into a single bitset without padding between inputs
     *
     * @tparam U Finn Datatype of input data
     * @tparam invertDirection Flag to reverse bit direction
     * @param input vector of bitsets to be merged
     * @return DynamicBitset Merged bitset
     */
    template<IsDatatype U>
    DynamicBitset mergeBitsets(const Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>>& input) {
        constexpr std::size_t bits = U().bitwidth();
        const std::size_t outputSize = input.size() * bits;
        // const std::size_t numThreads = std::min(static_cast<std::size_t>(omp_get_num_procs()), input.size() / 100);
        DynamicBitset ret(outputSize);

        // #pragma omp parallel for schedule(guided) shared(input, outputSize) reduction(bitsetOR : ret) default(none) num_threads(numThreads) if (input.size() > (static_cast<std::size_t>(omp_get_num_procs()) * 100))
        for (std::size_t i = 0; i < input.size(); ++i) {
            ret.setByte(input[i], i * bits);
        }
        return ret;
    }

    /**
     * @brief Converts a dynamic bitset into a vector of data
     *
     * @tparam T Type of data storage format (usually uint8_t aka byte)
     * @param input bitset to be converted
     * @return Finn::vector<T> Vector of data (usually vector of bytes)
     */
    template<typename T = uint8_t>
    Finn::vector<T> bitsetToByteVector(DynamicBitset& input) {
        return input.getStorageVec();
    }

    /**
     * @brief Internal implementations. Should not be used by user.
     *
     */
    namespace detail {
        /**
         * @brief Implementation of packing a single preprocessed range into a vector of bytes without padding
         *
         * @tparam U Finn Datatype of input data
         * @tparam IteratorType
         * @param first iterator pointing to first element of input
         * @param last iterator pointing to last element of input
         * @return Finn::vector<uint8_t> Vector of packed bytes
         */
        template<IsDatatype U, typename IteratorType>
        Finn::vector<uint8_t> packImpl(IteratorType first, IteratorType last) {
            constexpr size_t bitw = U().bitwidth();
            if constexpr (bitw == 8) {                      // FINN Datatype is a byte long
                return Finn::vector<uint8_t>(first, last);  // It fits exactly in a byte, so casting should be fine
            }
            else {
                // TODO(linusjun): For full bytes this is maybe overkill. So change it?
                auto bitsetvector = toBitset<U, true, false>(first, last);
                auto mergedBitset = mergeBitsets<U>(bitsetvector);
                return bitsetToByteVector<uint8_t>(mergedBitset);
            }
        }
    }  // namespace detail


    /**
     * @brief Function to pack a vector of U stored in T into a vector of bytes without padding bits inbetween
     *
     * @tparam U Finn Datatype of input data
     * @tparam IteratorType
     * @param first iterator pointing to first element of input
     * @param last iterator pointing to last element of input
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename IteratorType>
    Finn::vector<uint8_t> pack(IteratorType first, IteratorType last) {
        using T = typename std::iterator_traits<IteratorType>::value_type;
        if constexpr (std::endian::native == std::endian::big) {
            [] <bool flag = false>() { static_assert(flag, "Big-endian architectures are currently not supported!"); }
            ();
        }
        else if constexpr (std::endian::native == std::endian::little) {
            constexpr bool isFix = U().isFixedPoint();
            constexpr bool isInt = U().isInteger();
            if constexpr (isFix) {  // Datatype is Fixed Point Number
                constexpr std::size_t bytes = static_cast<std::size_t>(FinnUtils::ceil(static_cast<double>(U().bitwidth()) / 8.0));
                if constexpr (std::is_floating_point_v<T>) {  // floating point T have no shift operation, so replace with multiplication
                    std::transform(first, last, first, [](const T& val) { return val * (1 << U().fracBits()); });
                }
                else {
                    std::transform(first, last, first, [](const T& val) { return val << U().fracBits(); });
                }

                // Use smallest possible datatype for storing data
                using FourBytesOrLonger = typename std::conditional<bytes <= 4, uint32_t, uint64_t>::type;
                using TwoBytesOrLonger = typename std::conditional<bytes == 2, uint16_t, FourBytesOrLonger>::type;
                using OneByteOrLonger = typename std::conditional<bytes == 1, uint8_t, TwoBytesOrLonger>::type;

                Finn::vector<OneByteOrLonger> vec(first, last);
                return detail::packImpl<DatatypeInt<U().bitwidth()>>(vec.begin(), vec.end());
            }
            else if constexpr (std::is_floating_point_v<T> && isInt) {  // Datatype is integer number stored in floating point inputs
                // Use smallest possible datatype for storing data
                if constexpr (sizeof(T) >= 4 || sizeof(T) <= 8) {
                    using VecType = typename std::conditional<sizeof(T) == 4, uint32_t, uint64_t>::type;
                    Finn::vector<VecType> input(first, last);
                    return detail::packImpl<U>(input.begin(), input.end());
                }
                else {
                    [] <bool flag = false>() { static_assert(flag, "Weird floating point data length. Not supported!"); }
                    ();
                }
            }
            else if constexpr (std::is_floating_point_v<T>) {  // Datatype is floating point number
                static_assert(sizeof(float) == 4 && sizeof(double) == 8 && std::numeric_limits<T>::is_iec559, "Floating point format is not iee754 or unexpected type width!");
                if constexpr (sizeof(T) == 8) {            // FINN only supports 32 bit floating point numbers
                    Finn::vector<float> vec(first, last);  // This cast is necessary to make sure, that data is in a 32-bit floating point format before the bitcast.
                    Finn::vector<uint32_t> input;
                    input.resize(static_cast<std::size_t>(std::distance(first, last)));
                    std::transform(vec.begin(), vec.end(), input.begin(), [](const float& val) { return std::bit_cast<uint32_t>(val); });
                    return detail::packImpl<U>(input.begin(), input.end());
                }
                else if constexpr (sizeof(T) == 4) {
                    Finn::vector<uint32_t> input;
                    input.resize(static_cast<std::size_t>(std::distance(first, last)));
                    std::transform(first, last, input.begin(), [](const T& val) { return std::bit_cast<uint32_t>(val); });
                    return detail::packImpl<U>(input.begin(), input.end());
                }
                else {
                    [] <bool flag = false>() { static_assert(flag, "Weird floating point data length. Not supported!"); }
                    ();
                }
            }
            else if constexpr (!isInt && std::is_integral_v<T>) {  // Datatype is float stored in ints
                Finn::vector<float> vec(first, last);                // This cast is necessary to make sure, that data is in a 32-bit floating point format before the bitcast.
                Finn::vector<uint32_t> input;
                input.resize(static_cast<std::size_t>(std::distance(first, last)));
                std::transform(vec.begin(), vec.end(), input.begin(), [](const float& val) { return std::bit_cast<uint32_t>(val); });  // This conversion can overflow, but thats the user responsibility
                return detail::packImpl<U>(input.begin(), input.end());

            }
            else {  // Everything else
                return detail::packImpl<U>(first, last);
            }
        }
        else {
            [] <bool flag = false>() { static_assert(flag, "Mixed-endian architectures are currently not supported!"); }
            ();
        }
    }

    /**
     * @brief Function to pack a vector of U stored in T into a vector of bytes without padding bits inbetween
     *
     * @tparam U Finn Datatype of input data
     * @tparam T
     * @param foldedVec Vector of data to be packed
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename T>
    Finn::vector<uint8_t> pack(Finn::vector<T>& foldedVec) {
        if constexpr (U().bitwidth() == 8 && std::is_same<T, uint8_t>::value) {
            return foldedVec;
        }
        return pack<U>(foldedVec.begin(), foldedVec.end());
    }

    /**
     * @brief Function for template meta code. Tests if a FinnDatatype and C++ are compatible
     *
     * @tparam U FinnDatatype
     * @tparam T C++ Datatype
     * @return consteval Not available at runtime
     */
    template<IsDatatype U, typename T>
    consteval bool IsCorrectFinnType() {
        return std::is_floating_point_v<T> == !U().isInteger() && std::is_signed_v<T> == U().sign() && U().bitwidth() <= sizeof(T) * 8 && (U().isInteger() || std::is_same<float, T>::value) &&
            (std::is_floating_point_v<T> || std::is_integral_v<T>);
    }

    /**
     * @brief Function to pack multi dimensional input arrays
     *
     * @tparam U Finn Datatype of input data
     * @tparam IteratorType
     * @param first Iterator to first element of input
     * @param last  Iterator to last element of input
     * @param dynamicSpan DynamicMdSpan object that describes the folded structure of the input
     * @param elementsInnerMostDim number of elements in the inner most dimension
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename IteratorType>
    Finn::vector<uint8_t> packMultiDimensionalInputs(IteratorType first, IteratorType last, const Finn::DynamicMdSpan<IteratorType>& dynamicSpan, const std::size_t elementsInnerMostDim) {
        auto innerVecs = dynamicSpan.getMostInnerDims();
        std::size_t innerVecSize = innerVecs.size();

        // preallocate memory to make copy more efficient
        const std::size_t payloadBitsPerInnerDim = elementsInnerMostDim * U().bitwidth();
        constexpr std::size_t byte = 8;
        const std::size_t neededBytesPerInnerDim = (payloadBitsPerInnerDim % byte) ? payloadBitsPerInnerDim / byte + 1 : payloadBitsPerInnerDim / byte;  // ceil(payloadBitsPerInnerDim/8)
        const std::size_t neededBytesTotal = neededBytesPerInnerDim * innerVecSize;

        Finn::vector<uint8_t> packedMerged(neededBytesTotal);
        std::size_t threadcount = std::min({ (innerVecSize >> 5), static_cast<std::size_t>(omp_get_num_procs()), FinnUtils::log2_64(innerVecSize) << 1 });
        omp_set_num_threads(threadcount);
        //        std::cout << (std::min({(innerVecSize >> 5), static_cast<std::size_t>(omp_get_num_procs()), FinnUtils::log2_64(innerVecSize2)<<1})) << "\n";

                // for each most inner dimension
#pragma omp parallel for
        for (std::size_t i = 0; i < innerVecSize; ++i) {
            auto packed = Finn::pack<U>(innerVecs[i].begin(), innerVecs[i].end());
            // combine packing results
            std::copy(packed.begin(), packed.end(), packedMerged.begin() + i * neededBytesPerInnerDim);
        }

        return packedMerged;
    }


    /**
     * @brief Unpacks a byte vector into a vector of T containing U.
     *
     * @tparam U FinnDatatype that is contained in byte array
     * @tparam T Type of return vector. Is usually autodeduced, but it is also supported to use larger types for outputs: ex.: uint16_t instead of uint8_t is valid.
     * @tparam typename Unnamed template param is used to enable the function only for supported types
     * @param inp Byte vector
     * @param padding Number of padding bits inserted into last byte of input
     * @return Finn::vector<T> Vector of T containing U
     */
    template<IsDatatype U, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpack(std::span<uint8_t>& inp, std::size_t padding = 0) {
        static_assert(U().bitwidth() <= 64, "Finn Datatypes with more than 64 bit are not supported!");

        constexpr std::size_t neededBytes = FinnUtils::ceil(U().bitwidth() / 8.0);

        using FourBytesOrLonger = typename std::conditional<neededBytes <= 4, int32_t, int64_t>::type;
        using TwoBytesOrLonger = typename std::conditional<neededBytes == 2, int16_t, FourBytesOrLonger>::type;
        using FixedPointType = typename std::conditional<neededBytes == 1, int8_t, TwoBytesOrLonger>::type;
        using RetType = typename std::conditional<U().isFixedPoint(), FixedPointType, T>::type;

        if (inp.empty()) {
            FinnUtils::logAndError<std::runtime_error>("Input to unpacking operation is empty! Abord.");
        }

        if constexpr (reverseByte) {
            std::reverse(inp.begin(), inp.end());
        }

        if ((inp.size() * 8 - padding) % U().bitwidth() != 0) {
            FinnUtils::logAndError<std::runtime_error>("Amount of input elements is not a multiple of output elements");
        }

        constexpr size_t bitw = U().bitwidth();
        constexpr bool isSigned = U().sign();
        if constexpr (bitw / 8.0 == neededBytes) {  // complete Bytes, therefore no padding after here
            Finn::vector<RetType> ret(inp.size() / neededBytes, 0);
            for (std::size_t i = 0; i < ret.size(); ++i) {
                const std::size_t offset = i * neededBytes;
                if constexpr (isSigned) {  // TODO(linusjun): Test if this needs to be optimized or put into a seperate loop to allow vectorization.
                    if ((-128 & inp[offset + neededBytes - 1]) != 0) {
                        ret[i] = -1;
                    }
                }
                std::memcpy(&ret.data()[i], &inp.data()[offset], neededBytes);
            }
            constexpr bool isFixed = U().isFixedPoint();
            if constexpr (isFixed) {
                Finn::vector<float> fixedRet(ret.size());
                std::transform(ret.begin(), ret.end(), fixedRet.begin(), [](const RetType& val) { return static_cast<float>(val) / (1 << U().fracBits()); });
                return fixedRet;
            }
            else {
                return ret;
            }


        }
        else {
            constexpr std::size_t bitwidth = U().bitwidth();

            using FourBytesOrLongerUnsigned = typename std::conditional<bitwidth <= 32, uint64_t, __uint128_t>::type;
            using TwoBytesOrLongerUnsigned = typename std::conditional<bitwidth <= 16, uint32_t, FourBytesOrLongerUnsigned>::type;
            using BufferType = typename std::conditional<bitwidth <= 8, uint16_t, TwoBytesOrLongerUnsigned>::type;

            constexpr BufferType mask = createMask<BufferType>(bitwidth);
            const std::size_t elementsInInput = ((inp.size() * 8) - padding) / U().bitwidth();
            Finn::vector<RetType> ret(elementsInInput, 0);

            for (std::size_t index = 0; index < elementsInInput; ++index) {
                const std::size_t lowerBit = index * bitwidth;
                const std::size_t lowerBorderByte = lowerBit / 8;                   // Intentionally rounding down
                const std::size_t upperBorderByte = (lowerBit + bitwidth - 1) / 8;  // Intentionally rounding down
                const std::size_t numBytes = upperBorderByte - lowerBorderByte + 1;
                const std::size_t shiftOffset = lowerBit - (lowerBorderByte * 8);

                BufferType buffer = 0;                                         // This buffer is big enough to contain two FinnDatatype elements. Therefore no problem if one FinnDatatype element is shifted.
                std::memcpy(&buffer, &inp.data()[lowerBorderByte], numBytes);  // Fill Buffer with from byte inputs
                buffer = static_cast<BufferType>(buffer >> shiftOffset);       // remove remaining bits from previous element
                buffer &= mask;                                                // remove bits from next element

                constexpr bool isSigned = U().sign();
                if constexpr (isSigned) {  // TODO(linusjun): Test if this needs to be optimized or put into a seperate loop to allow vectorization.
                    if (((BufferType(1U) << (bitwidth - 1)) & buffer) != 0) {
                        buffer |= ~mask;
                    }
                }
                ret[index] = static_cast<RetType>(buffer);
            }

            constexpr bool isFixed = U().isFixedPoint();
            if constexpr (isFixed) {
                Finn::vector<float> fixedRet(ret.size());
                std::transform(ret.begin(), ret.end(), fixedRet.begin(), [](const RetType& val) { return static_cast<float>(static_cast<FixedPointType>(val)) / (1 << U().fracBits()); });
                return fixedRet;
            }
            else {
                return ret;
            }
        }
    }

    /**
     * @brief Unpacks a byte vector into a vector of T containing U.
     *
     * @tparam U FinnDatatype that is contained in byte array
     * @tparam T Type of return vector. Is usually autodeduced, but it is also supported to use larger types for outputs: ex.: uint16_t instead of uint8_t is valid.
     * @tparam typename Unnamed template param is used to enable the function only for supported types
     * @param inp Byte vector
     * @param padding Number of padding bits inserted into last byte of input
     * @return Finn::vector<T> Vector of T containing U
     */
    template<IsDatatype U, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpack(Finn::vector<uint8_t>& inp, std::size_t padding = 0) {
        std::span<uint8_t> spa{ inp.begin(), inp.end() };
        return unpack<U, reverseByte, T>(spa, padding);
    }

    /**
     * @brief Unpacks multi-dimensional output vectors
     *
     * @tparam U FinnDatatype that is contained in byte array
     * @tparam IteratorType Iterator of container containing uint8_t
     * @tparam reverseByte Switch to reverse input vectors
     * @tparam T Type of return vector. Usually autodeduced.
     * @param begin Iterator to first element of linearized byte array
     * @param end Iterator to the end of linearized byte array
     * @param dynSpan DynamicMdSpan describing the structure of the byte array
     * @param foldedShape Shape of the target vector
     * @return Finn::vector<T> Vector of T containing U
     */
    template<IsDatatype U, std::input_iterator IteratorType, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpackMultiDimensionalOutputs(IteratorType begin, IteratorType end, const Finn::DynamicMdSpan<IteratorType>& dynSpan, const shapeFolded_t& foldedShape)
        requires(std::is_same_v<uint8_t, typename std::iterator_traits<IteratorType>::value_type>)
    {
        constexpr std::size_t bytes = 8;
        auto innerDimVecs = dynSpan.getMostInnerDims();
        std::size_t innerVecSize = innerDimVecs.size();
        const std::size_t padding = innerDimVecs[0].size() * bytes - foldedShape.back() * U().bitwidth();

        // preallocate memory to make copy more efficient
        const std::size_t retSizeTotal = FinnUtils::shapeToElements(foldedShape);
        Finn::vector<T> unpackedMerged(retSizeTotal);

        #pragma omp parallel for
        for (std::size_t i = 0; i < innerDimVecs.size(); ++i) {
            auto unpacked = Finn::unpack<U>(innerDimVecs[i], padding);
            std::copy(unpacked.begin(), unpacked.end(), unpackedMerged.begin() + i * foldedShape.back());
        }

        return unpackedMerged;
    }

}  // namespace Finn

#endif  // DATAPACKING
