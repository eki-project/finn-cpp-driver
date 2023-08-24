#ifndef _SRC_UTILS_TYPES_H_
#define _SRC_UTILS_TYPES_H_

#include <variant>
#include <vector>

enum class PLATFORM { ALVEO = 0, INVALID = -1 };

enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2, INVALID = -1 };

enum class BUFFER_OP_RESULT { SUCCESS = 0, FAILURE = -1, OVER_BOUNDS_WRITE = -2, OVER_BOUNDS_READ = -3 };

enum class TRANSFER_MODE { MEMORY_BUFFERED = 0, STREAMED = 1, INVALID = -1 };

enum class IO_SWITCH { INPUT = 0, OUTPUT = 1, INOUT = 2 };  // General purpose, no specific usecase

// Forward Declarations
template<typename T>
struct MemoryMap;

using shape_t = std::vector<unsigned int>;
using shape_list_t = std::vector<shape_t>;
using bytewidth_list_t = std::vector<unsigned int>;

/**
 * @brief Describes a list (initializer_list) of types shape_t OR MemoryMap<T>. In the first case a buffer with the appropiate byte size is simply created, with the latter, an existing memory mapped is taken as input for chaining FPGAs
 * together
 *
 * @tparam T The datatype of the DeviceHandler
 */
template<typename T>
using BOMemoryDefinitionArguments = std::vector<std::variant<shape_t, MemoryMap<T>>>;

#endif  // _SRC_UTILS_TYPES_H_