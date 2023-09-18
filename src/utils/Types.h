#ifndef TYPES_H
#define TYPES_H

#include <cctype>
#include <variant>
#include <vector>

enum class PLATFORM { ALVEO = 0, INVALID = -1 };

enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2, INVALID = -1 };

enum class BUFFER_OP_RESULT { SUCCESS = 0, FAILURE = -1, OVER_BOUNDS_WRITE = -2, OVER_BOUNDS_READ = -3 };

enum class TRANSFER_MODE { MEMORY_BUFFERED = 0, STREAMED = 1, INVALID = -1 };

enum class IO { INPUT = 0, OUTPUT = 1, INOUT = 2, UNSPECIFIED = -1 };  // General purpose, no specific usecase

enum class SIZE_SPECIFIER { BYTES = 0, ELEMENTS = 1, NUMBERS = 2, SAMPLES = 3, PARTS = 4, ELEMENTS_PER_PART = 5, INVALID = -1 };

enum class ENDIAN { LITTLE = 0, BIG = 1, UNSPECIFIED = -1 };

// Forward Declarations
template<typename T>
struct MemoryMap;

using shapeNormal_t = std::vector<unsigned int>;
using shapeFolded_t = std::vector<unsigned int>;
using shapePacked_t = std::vector<unsigned int>;
using shape_t = std::vector<unsigned int>;

// using shape_list_t = std::vector<shape_t>;
using bytewidth_list_t = std::vector<unsigned int>;

using size_bytes_t = std::size_t;
using index_t = long unsigned int;
#endif  // TYPES_H