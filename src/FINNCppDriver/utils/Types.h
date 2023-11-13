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

#ifndef TYPES_H
#define TYPES_H

#include <FINNCppDriver/utils/AlignedAllocator.hpp>
#include <vector>

namespace Finn {
    template<typename T>
    using vector = std::vector<T, AlignedAllocator<T>>;
}  // namespace Finn

enum class PLATFORM { ALVEO = 0, INVALID = -1 };

enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2, INVALID = -1 };

enum class BUFFER_OP_RESULT { SUCCESS = 0, FAILURE = -1, OVER_BOUNDS_WRITE = -2, OVER_BOUNDS_READ = -3 };

enum class TRANSFER_MODE { MEMORY_BUFFERED = 0, STREAMED = 1, INVALID = -1 };

enum class IO { INPUT = 0, OUTPUT = 1, INOUT = 2, UNSPECIFIED = -1 };  // General purpose, no specific usecase

enum class SIZE_SPECIFIER { BYTES = 0, ELEMENTS = 1, NUMBERS = 2, SAMPLES = 3, PARTS = 4, ELEMENTS_PER_PART = 5, VALUES_PER_INPUT = 6, INVALID = -1 };

enum class ENDIAN { LITTLE = 0, BIG = 1, UNSPECIFIED = -1 };

using shapeNormal_t = std::vector<unsigned int>;
using shapeFolded_t = std::vector<unsigned int>;
using shapePacked_t = std::vector<unsigned int>;
using shape_t = std::vector<unsigned int>;

using size_bytes_t = std::size_t;
using index_t = long unsigned int;
#endif  // TYPES_H