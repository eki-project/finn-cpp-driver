/**
 * @file Types.h
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Various type definitions used in the FINN C++ driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef TYPES
#define TYPES

#include <FINNCppDriver/utils/AlignedAllocator.hpp>
#include <vector>

// TODO(linusjun): Clean up this file. Half of these types are no longer used...

using uint = unsigned int;

namespace Finn {
    /**
     * @brief Defines Finn internal vector
     *
     * @tparam T
     */
    template<typename T>
    using vector = std::vector<T, AlignedAllocator<T>>;
}  // namespace Finn

/**
 * @brief Platform enum
 *
 */
enum class PLATFORM { ALVEO = 0, INVALID = -1 };

/**
 * @brief Driver mode enum
 *
 */
enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

/**
 * @brief Shapetype enum
 *
 */
enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2, INVALID = -1 };

/**
 * @brief Buffer operation result type
 *
 */
enum class BUFFER_OP_RESULT { SUCCESS = 0, FAILURE = -1, OVER_BOUNDS_WRITE = -2, OVER_BOUNDS_READ = -3 };

/**
 * @brief Buffer transfer mode
 *
 */
enum class TRANSFER_MODE { MEMORY_BUFFERED = 0, STREAMED = 1, INVALID = -1 };

/**
 * @brief IO mode; General purpose, no specific usecase
 *
 */
enum class IO { INPUT = 0, OUTPUT = 1, INOUT = 2, UNSPECIFIED = -1 };

/**
 * @brief Size specifier enum
 *
 */
enum class SIZE_SPECIFIER { BYTES = 0, TOTAL_DATA_SIZE = 1, BATCHSIZE = 4, FEATUREMAP_SIZE = 5, INVALID = -1 };

/**
 * @brief Endianness
 *
 */
enum class ENDIAN { LITTLE = 0, BIG = 1, UNSPECIFIED = -1 };

/**
 * @brief Type for normal Shape
 *
 */
using shapeNormal_t = std::vector<unsigned int>;

/**
 * @brief Type for folded Shape
 *
 */
using shapeFolded_t = std::vector<unsigned int>;

/**
 * @brief Type for packed Shape
 *
 */
using shapePacked_t = std::vector<unsigned int>;

/**
 * @brief General type to store shapes
 *
 */
using shape_t = std::vector<unsigned int>;

/**
 * @brief Type for byte lengths
 *
 */
using size_bytes_t = std::size_t;

/**
 * @brief Index type
 *
 */
using index_t = long unsigned int;
#endif  // TYPES
