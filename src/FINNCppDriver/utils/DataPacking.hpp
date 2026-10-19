/**
 * @file DataPacking.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements data packing and unpacking efficiently
 * @version 0.2
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 * Packed format: a continuous little-endian bitstream. Element i of a FINN datatype with bitwidth b occupies bits
 * [i*b, (i+1)*b), bit 0 of the stream is bit 0 of byte 0, and the last byte is zero-padded. Signed types are stored as
 * two's complement in b bits, Bipolar stores -1 as 0 and +1 as 1, fixed-point types store the value scaled by
 * 2^fracBits, and FLOAT32 stores the IEEE-754 bit pattern.
 */

#ifndef DATAPACKING
#define DATAPACKING

#include <FINNCppDriver/utils/CustomDynamicBitset.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Types.h>
#include <omp.h>

#include <FINNCppDriver/utils/AlignedAllocator.hpp>
#include <FINNCppDriver/utils/DynamicMdSpan.hpp>
#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <algorithm>
#include <bit>
#include <bitset>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <type_traits>

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
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                T const mask = T{0xFF} << (i * 8);
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
            uint8_t operator()(const uint8_t x) const { return Detail{}(x); }
            uint16_t operator()(const uint16_t x) const { return bitshuffleMemCopy<Detail, uint16_t>(x); }
            uint32_t operator()(const uint32_t x) const { return bitshuffleMemCopy<Detail, uint32_t>(x); }
            uint64_t operator()(const uint64_t x) const { return bitshuffleMemCopy<Detail, uint64_t>(x); }
            int8_t operator()(const int8_t x) const { return static_cast<int8_t>(Detail{}(static_cast<uint8_t>(x))); }
            int16_t operator()(const int16_t x) const { return static_cast<int16_t>(bitshuffleMemCopy<Detail, uint16_t>(static_cast<uint16_t>(x))); }
            int32_t operator()(const int32_t x) const { return static_cast<int32_t>(bitshuffleMemCopy<Detail, uint32_t>(static_cast<uint32_t>(x))); }
            int64_t operator()(const int64_t x) const { return static_cast<int64_t>(bitshuffleMemCopy<Detail, uint64_t>(static_cast<uint64_t>(x))); }
        };

    }  // namespace bitshuffling

    /**
     * @brief Namespace to autodeduce return type based on Finn Datatype
     *
     */
    namespace UnpackingAutoRetType {
        template<IsDatatype U>
        using FourBytesOrLongerSigned = typename std::conditional<U().bitwidth() <= 32, int32_t, int64_t>::type;

        template<IsDatatype U>
        using TwoBytesOrLongerSigned = typename std::conditional<U().bitwidth() <= 16, int16_t, FourBytesOrLongerSigned<U>>::type;

        /**
         * @brief Smallest signed integer type that holds U
         */
        template<IsDatatype U>
        using SignedRetType = typename std::conditional<U().bitwidth() <= 8, int8_t, TwoBytesOrLongerSigned<U>>::type;

        template<IsDatatype U>
        using FourBytesOrLongerUnsigned = typename std::conditional<U().bitwidth() <= 32, uint32_t, uint64_t>::type;

        template<IsDatatype U>
        using TwoBytesOrLongerUnsigned = typename std::conditional<U().bitwidth() <= 16, uint16_t, FourBytesOrLongerUnsigned<U>>::type;

        /**
         * @brief Smallest unsigned integer type that holds U
         */
        template<IsDatatype U>
        using UnsignedRetType = typename std::conditional<U().bitwidth() <= 8, uint8_t, TwoBytesOrLongerUnsigned<U>>::type;

        /**
         * @brief Returns a unsigned integer type if input U unsigned and a signed type otherwise
         */
        template<IsDatatype U>
        using IntegralType = typename std::conditional<U().sign(), SignedRetType<U>, UnsignedRetType<U>>::type;

        /**
         * @brief Returns a integer type if input U integer type and float otherwise
         */
        template<IsDatatype U>
        using AutoRetType = typename std::conditional<U().isInteger(), IntegralType<U>, float>::type;
    }  // namespace UnpackingAutoRetType

    /**
     * @brief Creates a bitmask for the lower bits
     *
     * @tparam T Datatype of mask
     * @param bits Number of low bits to set
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
     * @brief Internal implementations. Should not be used by user.
     *
     */
    namespace detail {
        /// Elements below this count are packed/unpacked on the calling thread
        inline constexpr std::size_t parallelThreshold = 1U << 16;

        /**
         * @brief Number of OpenMP threads worth using for `elements` elements: at least 32k elements per thread
         */
        inline int packingThreads(std::size_t elements) {
            const auto wanted = static_cast<int>(std::min<std::size_t>(elements / (1U << 15), static_cast<std::size_t>(omp_get_max_threads())));
            return std::max(1, wanted);
        }

        /**
         * @brief Number of bytes needed to store `elements` elements of `bitwidth` bits as a continuous bitstream
         */
        constexpr std::size_t packedBytes(std::size_t elements, std::size_t bitwidth) { return FinnUtils::fastDivCeil(elements * bitwidth, std::size_t{8}); }

        /// Unsigned type holding the raw bit pattern of one element
        template<std::size_t bitwidth>
        using RawType = std::conditional_t<(bitwidth <= 8), uint8_t, std::conditional_t<(bitwidth <= 16), uint16_t, std::conditional_t<(bitwidth <= 32), uint32_t, uint64_t>>>;

        /// Accumulator wide enough for one element plus up to 7 pending bits
        template<std::size_t bitwidth>
        using Accumulator = std::conditional_t<(bitwidth <= 56), uint64_t, unsigned __int128>;

        /**
         * @brief Sign-extends the low `bitwidth` bits of raw
         */
        template<std::size_t bitwidth>
        constexpr int64_t signExtend(uint64_t raw) {
            constexpr unsigned shift = 64 - bitwidth;
            return static_cast<int64_t>(raw << shift) >> shift;
        }

        /**
         * @brief Converts one element of storage type T holding a U into the bit pattern that is stored for it
         *
         * Never modifies the input and handles negative values of narrow signed storage types correctly.
         */
        template<IsDatatype U, typename T>
        inline RawType<U().bitwidth()> toRaw(const T& value) {
            constexpr std::size_t bitwidth = U().bitwidth();
            constexpr uint64_t mask = (bitwidth >= 64) ? ~uint64_t{0} : ((uint64_t{1} << bitwidth) - 1);
            using Raw = RawType<bitwidth>;
            if constexpr (U().isFixedPoint()) {
                // Scale into the integer grid; truncation toward zero matches a plain shift of the integer value
                int64_t scaled = 0;
                if constexpr (std::is_floating_point_v<T>) {
                    scaled = static_cast<int64_t>(value * static_cast<T>(uint64_t{1} << U().fracBits()));
                } else {
                    scaled = static_cast<int64_t>(value) * static_cast<int64_t>(uint64_t{1} << U().fracBits());
                }
                return static_cast<Raw>(static_cast<uint64_t>(scaled) & mask);
            } else if constexpr (!U().isInteger()) {  // FINN FLOAT32: store the IEEE-754 bit pattern of the float value
                return static_cast<Raw>(std::bit_cast<uint32_t>(static_cast<float>(value)));
            } else if constexpr (std::is_same_v<U, DatatypeBipolar>) {  // -1 -> 0, +1 -> 1
                return static_cast<Raw>(value > 0 ? 1 : 0);
            } else if constexpr (std::is_floating_point_v<T> || std::is_signed_v<T>) {
                return static_cast<Raw>(static_cast<uint64_t>(static_cast<int64_t>(value)) & mask);
            } else {
                return static_cast<Raw>(static_cast<uint64_t>(value) & mask);
            }
        }

        /**
         * @brief Converts the stored bit pattern of one element back into a value of type T
         */
        template<IsDatatype U, typename T>
        inline T fromRaw(RawType<U().bitwidth()> raw) {
            constexpr std::size_t bitwidth = U().bitwidth();
            if constexpr (U().isFixedPoint()) {
                return static_cast<T>(static_cast<float>(signExtend<bitwidth>(raw)) / static_cast<float>(uint64_t{1} << U().fracBits()));
            } else if constexpr (!U().isInteger()) {
                return static_cast<T>(std::bit_cast<float>(static_cast<uint32_t>(raw)));
            } else if constexpr (std::is_same_v<U, DatatypeBipolar>) {
                return raw != 0 ? static_cast<T>(1) : static_cast<T>(-1);
            } else if constexpr (U().sign()) {
                return static_cast<T>(signExtend<bitwidth>(raw));
            } else {
                return static_cast<T>(raw);
            }
        }

        /**
         * @brief Packs n elements as a continuous bitstream into dst (fully overwritten, packedBytes(n, bitwidth) bytes)
         *
         * @param convert Callable turning one input element into its RawType<bitwidth> bit pattern
         */
        template<std::size_t bitwidth, typename InputIt, typename Convert>
        inline void packRange(InputIt first, std::size_t n, uint8_t* dst, Convert convert) {
            if constexpr (bitwidth % 8 == 0) {
                constexpr std::size_t bytes = bitwidth / 8;
                for (std::size_t i = 0; i < n; ++i, ++first) {
                    const auto raw = convert(*first);
                    std::memcpy(dst + i * bytes, &raw, bytes);  // little endian: the low bytes come first
                }
            } else {
                using Acc = Accumulator<bitwidth>;
                Acc acc = 0;
                unsigned pending = 0;
                std::size_t out = 0;
                for (std::size_t i = 0; i < n; ++i, ++first) {
                    acc |= static_cast<Acc>(convert(*first)) << pending;
                    pending += bitwidth;
                    while (pending >= 8) {
                        dst[out++] = static_cast<uint8_t>(acc);
                        acc >>= 8;
                        pending -= 8;
                    }
                }
                if (pending > 0) {
                    dst[out] = static_cast<uint8_t>(acc);
                }
            }
        }

        /**
         * @brief Unpacks n elements from a continuous bitstream
         *
         * @param emit Callable receiving (index, RawType<bitwidth> raw) for every element
         */
        template<std::size_t bitwidth, typename Emit>
        inline void unpackRange(const uint8_t* src, std::size_t n, Emit emit) {
            using Raw = RawType<bitwidth>;
            if constexpr (bitwidth % 8 == 0) {
                constexpr std::size_t bytes = bitwidth / 8;
                for (std::size_t i = 0; i < n; ++i) {
                    Raw raw = 0;
                    std::memcpy(&raw, src + i * bytes, bytes);
                    emit(i, raw);
                }
            } else {
                using Acc = Accumulator<bitwidth>;
                constexpr Acc mask = (static_cast<Acc>(1) << bitwidth) - 1;
                Acc acc = 0;
                unsigned pending = 0;
                std::size_t in = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    while (pending < bitwidth) {
                        acc |= static_cast<Acc>(src[in++]) << pending;
                        pending += 8;
                    }
                    emit(i, static_cast<Raw>(acc & mask));
                    acc >>= bitwidth;
                    pending -= bitwidth;
                }
            }
        }

        /**
         * @brief Internal Implementation of toBitset (operates on a private copy of the input)
         */
        template<IsDatatype U, bool invertBytes = true, bool reverseBits = true, typename T>
        Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> toBitsetImpl(Finn::vector<T> values) {
            if constexpr (reverseBits) {
                constexpr std::size_t shift = (sizeof(T) * 8 - U().bitwidth());
                bitshuffling::Wrapper wrap{};
                std::transform(values.begin(), values.end(), values.begin(), wrap);
                if constexpr (std::is_same_v<U, DatatypeBipolar>) {
                    std::transform(values.begin(), values.end(), values.begin(), [](const T& val) { return (val + 1) >> (shift - 1); });  // This converts bipolar to binary
                } else {
                    std::transform(values.begin(), values.end(), values.begin(), [](const T& val) { return val >> shift; });
                }
            } else {
                if constexpr (std::is_same_v<U, DatatypeBipolar>) {
                    std::transform(values.begin(), values.end(), values.begin(), [](const T& val) { return (val + 1) >> 1; });  // This converts bipolar to binary
                }
            }
            constexpr T mask = createMask<T>(U().bitwidth());
            std::transform(values.begin(), values.end(), values.begin(), [](const T& val) { return val & mask; });  // Cut away all bits larger than U().bitwidth()
            if constexpr (invertBytes) {
                return Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>>(values.begin(), values.end());
            } else {
                return Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>>(values.rbegin(), values.rend());
            }
        }
    }  // namespace detail


    /**
     * @brief Converts a range of inputs to a vector of bit patterns representing the inputs. The input is not modified.
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
        using T = std::remove_cv_t<typename std::iterator_traits<IteratorType>::value_type>;
        static_assert(sizeof(T) <= 8, "Datatypes with more than 8 bytes are currently not supported!");
        // Work on the unsigned type of the same size to avoid undefined behavior of shift operations on negative values
        using FourBytesOrLonger = typename std::conditional<sizeof(T) <= 4, uint32_t, uint64_t>::type;
        using TwoBytesOrLonger = typename std::conditional<sizeof(T) == 2, uint16_t, FourBytesOrLonger>::type;
        using Unsigned = typename std::conditional<sizeof(T) == 1, uint8_t, TwoBytesOrLonger>::type;
        Finn::vector<Unsigned> values;
        values.reserve(static_cast<std::size_t>(std::distance(first, last)));
        for (; first != last; ++first) {
            values.push_back(static_cast<Unsigned>(*first));
        }
        return detail::toBitsetImpl<U, invertBytes, reverseBits>(std::move(values));
    }

    /**
     * @brief Converts vector of inputs to a vector of bit patterns representing the inputs. The input is not modified.
     */
    template<IsDatatype U, bool invertBytes = true, bool reverseBits = true, std::integral V>
    Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>> toBitset(const Finn::vector<V>& input) {
        return toBitset<U, invertBytes, reverseBits>(input.begin(), input.end());
    }

    /**
     * @brief OpenMp OR reduction for DynamicBitsets
     *
     * @param inout Input and output dataset
     * @param in Input dataset
     */
    inline void bitsetOR(DynamicBitset& inout, DynamicBitset& in) { inout |= in; }
#pragma omp declare reduction(bitsetOR:DynamicBitset : bitsetOR(omp_out, omp_in)) initializer(omp_priv = omp_orig)

    /**
     * @brief Merges a vector of bitsets into a single bitset without padding between inputs
     *
     * @tparam U Finn Datatype of input data
     * @param input vector of bitsets to be merged
     * @return DynamicBitset Merged bitset
     */
    template<IsDatatype U>
    DynamicBitset mergeBitsets(const Finn::vector<UnpackingAutoRetType::UnsignedRetType<U>>& input) {
        constexpr std::size_t bits = U().bitwidth();
        DynamicBitset ret(input.size() * bits);
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
     * @brief Packs a range of U stored in T into a vector of bytes without padding bits in between. The input is not modified.
     *
     * @tparam U Finn Datatype of input data
     * @tparam IteratorType
     * @param first iterator pointing to first element of input
     * @param last iterator pointing to last element of input
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename IteratorType>
    Finn::vector<uint8_t> pack(IteratorType first, IteratorType last) {
        using T = std::remove_cv_t<typename std::iterator_traits<IteratorType>::value_type>;
        static_assert(std::endian::native == std::endian::little, "Big- and mixed-endian architectures are currently not supported!");
        static_assert(std::is_arithmetic_v<T>, "Only arithmetic storage types can be packed");
        constexpr std::size_t bitwidth = U().bitwidth();
        static_assert(bitwidth <= 64, "Finn Datatypes with more than 64 bit are not supported!");
        if constexpr (!U().isInteger() && !U().isFixedPoint()) {
            static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "Floating point format is not iee754 or unexpected type width!");
        }

        const std::size_t elements = static_cast<std::size_t>(std::distance(first, last));
        if constexpr (bitwidth == 8 && U().isInteger() && std::is_integral_v<T> && sizeof(T) == 1) {
            return Finn::vector<uint8_t>(first, last);  // Already in the packed format
        }
        Finn::vector<uint8_t> packed(detail::packedBytes(elements, bitwidth));
        detail::packRange<bitwidth>(first, elements, packed.data(), [](const T& value) { return detail::toRaw<U, T>(value); });
        return packed;
    }

    /**
     * @brief Packs a vector of U stored in T into a vector of bytes without padding bits in between. The input is not modified.
     *
     * @tparam U Finn Datatype of input data
     * @tparam T
     * @param foldedVec Vector of data to be packed
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename T>
    Finn::vector<uint8_t> pack(const Finn::vector<T>& foldedVec) {
        if constexpr (U().bitwidth() == 8 && std::is_same<T, uint8_t>::value) {
            return foldedVec;
        }
        return pack<U>(foldedVec.begin(), foldedVec.end());
    }

    /**
     * @brief Packs a multi dimensional input: every innermost row is packed on its own and padded to full bytes
     *
     * @tparam U Finn Datatype of input data
     * @tparam IteratorType
     * @param first Iterator to first element of input
     * @param last  Iterator to end of input
     * @param dynamicSpan DynamicMdSpan object that describes the folded structure of the input
     * @param elementsInnerMostDim number of elements in the inner most dimension
     * @return Finn::vector<uint8_t> Vector of packed bytes
     */
    template<IsDatatype U, typename IteratorType>
    Finn::vector<uint8_t> packMultiDimensionalInputs(IteratorType first, IteratorType last, const Finn::DynamicMdSpan<IteratorType>& dynamicSpan, const std::size_t elementsInnerMostDim) {
        constexpr std::size_t bitwidth = U().bitwidth();
        if constexpr (bitwidth % 8 == 0) {  // Rows of whole bytes never need padding, so the data can be packed as one range
            return Finn::pack<U>(first, last);
        } else {
            using T = std::remove_cv_t<typename std::iterator_traits<IteratorType>::value_type>;
            const std::size_t rows = dynamicSpan.innerCount();
            const std::size_t rowElements = dynamicSpan.innerStride();
            if (rowElements != elementsInnerMostDim) {
                Finn::logAndError<std::invalid_argument>("The innermost dimension of the span (" + std::to_string(rowElements) + ") does not match the given innermost dimension (" + std::to_string(elementsInnerMostDim) + ")");
            }
            const std::size_t bytesPerRow = detail::packedBytes(rowElements, bitwidth);
            const std::size_t total = rows * rowElements;
            Finn::vector<uint8_t> packedMerged(bytesPerRow * rows);
            uint8_t* out = packedMerged.data();

#pragma omp parallel for schedule(static) num_threads(detail::packingThreads(total)) if (total >= detail::parallelThreshold)
            for (std::size_t row = 0; row < rows; ++row) {
                detail::packRange<bitwidth>(std::next(first, static_cast<std::iter_difference_t<IteratorType>>(row * rowElements)), rowElements, out + row * bytesPerRow, [](const T& value) { return detail::toRaw<U, T>(value); });
            }
            return packedMerged;
        }
    }

    /**
     * @brief Unpacks a byte range into a vector of T containing U. The input is not modified.
     *
     * @tparam U FinnDatatype that is contained in byte array
     * @tparam reverseByte Interpret the input in reversed byte order
     * @tparam T Type of return vector. Is usually autodeduced, but larger types are also supported, e.g. int32_t for Int<4>.
     * @param inp Bytes
     * @param padding Number of padding bits at the end of the input
     * @return Finn::vector<T> Vector of T containing U
     */
    template<IsDatatype U, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpack(std::span<const uint8_t> inp, std::size_t padding = 0) {
        constexpr std::size_t bitwidth = U().bitwidth();
        static_assert(bitwidth <= 64, "Finn Datatypes with more than 64 bit are not supported!");

        if (inp.empty()) {
            Finn::logAndError<std::runtime_error>("Input to unpacking operation is empty! Abord.");
        }
        const std::size_t totalBits = inp.size() * 8;
        if (padding >= totalBits || (totalBits - padding) % bitwidth != 0) {
            Finn::logAndError<std::runtime_error>("Amount of input elements is not a multiple of output elements");
        }
        const std::size_t elements = (totalBits - padding) / bitwidth;

        Finn::vector<uint8_t> reversed;
        const uint8_t* src = inp.data();
        if constexpr (reverseByte) {
            reversed.assign(inp.rbegin(), inp.rend());
            src = reversed.data();
        }

        Finn::vector<T> ret(elements);
        T* out = ret.data();
        detail::unpackRange<bitwidth>(src, elements, [out](std::size_t index, detail::RawType<bitwidth> raw) { out[index] = detail::fromRaw<U, T>(raw); });
        return ret;
    }

    /**
     * @brief Unpacks a byte vector into a vector of T containing U. The input is not modified.
     */
    template<IsDatatype U, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpack(const Finn::vector<uint8_t>& inp, std::size_t padding = 0) {
        return unpack<U, reverseByte, T>(std::span<const uint8_t>(inp.data(), inp.size()), padding);
    }

    /**
     * @brief Unpacks a byte range given by iterators into a vector of T containing U. The input is not modified.
     */
    template<IsDatatype U, std::input_iterator IteratorType, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpack(IteratorType begin, IteratorType end, std::size_t padding = 0)
        requires(std::is_same_v<uint8_t, std::remove_cv_t<typename std::iterator_traits<IteratorType>::value_type>>)
    {
        return unpack<U, reverseByte, T>(std::span<const uint8_t>(std::to_address(begin), static_cast<std::size_t>(std::distance(begin, end))), padding);
    }

    /**
     * @brief Unpacks multi-dimensional output vectors: every innermost row of the byte array is unpacked on its own
     *
     * @tparam U FinnDatatype that is contained in byte array
     * @tparam IteratorType Iterator of container containing uint8_t
     * @tparam reverseByte Switch to reverse input rows
     * @tparam T Type of return vector. Usually autodeduced.
     * @param begin Iterator to first element of linearized byte array
     * @param end Iterator to the end of linearized byte array
     * @param dynSpan DynamicMdSpan describing the structure of the byte array (packed shape)
     * @param foldedShape Shape of the target vector
     * @return Finn::vector<T> Vector of T containing U
     */
    template<IsDatatype U, std::input_iterator IteratorType, bool reverseByte = false, typename T = UnpackingAutoRetType::AutoRetType<U>, typename = std::enable_if_t<IsCorrectFinnType<U, T>()>>
    Finn::vector<T> unpackMultiDimensionalOutputs(IteratorType begin, IteratorType end, const Finn::DynamicMdSpan<IteratorType>& dynSpan, const shapeFolded_t& foldedShape)
        requires(std::is_same_v<uint8_t, std::remove_cv_t<typename std::iterator_traits<IteratorType>::value_type>>)
    {
        constexpr std::size_t bitwidth = U().bitwidth();
        if constexpr (bitwidth % 8 == 0) {
            return Finn::unpack<U, IteratorType, reverseByte, T>(begin, end, 0);
        } else {
            const std::size_t rows = dynSpan.innerCount();
            const std::size_t bytesPerRow = dynSpan.innerStride();
            const std::size_t elementsPerRow = foldedShape.back();
            const std::size_t total = FinnUtils::shapeToElements(foldedShape);
            if (bytesPerRow * 8 < elementsPerRow * bitwidth || total != rows * elementsPerRow) {
                Finn::logAndError<std::invalid_argument>("Packed shape (" + std::to_string(rows) + " rows of " + std::to_string(bytesPerRow) + " bytes) cannot hold the folded shape (" + std::to_string(rows) + " rows of " +
                                                         std::to_string(elementsPerRow) + " elements of " + std::to_string(bitwidth) + " bit)");
            }

            Finn::vector<T> unpackedMerged(total);
            T* out = unpackedMerged.data();
            const uint8_t* src = std::to_address(begin);

#pragma omp parallel for schedule(static) num_threads(detail::packingThreads(total)) if (total >= detail::parallelThreshold)
            for (std::size_t row = 0; row < rows; ++row) {
                const uint8_t* rowSrc = src + row * bytesPerRow;
                Finn::vector<uint8_t> reversed;
                if constexpr (reverseByte) {
                    reversed.assign(std::make_reverse_iterator(rowSrc + bytesPerRow), std::make_reverse_iterator(rowSrc));
                    rowSrc = reversed.data();
                }
                T* rowOut = out + row * elementsPerRow;
                detail::unpackRange<bitwidth>(rowSrc, elementsPerRow, [rowOut](std::size_t index, detail::RawType<bitwidth> raw) { rowOut[index] = detail::fromRaw<U, T>(raw); });
            }
            return unpackedMerged;
        }
    }

}  // namespace Finn

#endif /* DATAPACKING */
