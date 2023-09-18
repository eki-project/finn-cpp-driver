#ifndef TYPES_H
#define TYPES_H

#include <cctype>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>

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

/**
 * @brief A small storage struct to manage the description of Buffers
 *
 */
struct BufferDescriptor {
    std::string kernelName;  // Kernel Name (e.g. vadd / idma0)
    std::string cuName;      // CU Name (e.g. vadd:{inst0} / idma0:{inst0})
    shape_t packedShape;
    IO ioMode;

    // TODO(bwintermann): Currently unused, reserved for multi-fpga usage
    unsigned int fpgaIndex = 0;
    unsigned int slrIndex = 0;

    BufferDescriptor(const std::string& pKernelName, const std::string& pCuName, const shape_t& pPackedShape, IO pIoMode) : kernelName(pKernelName), cuName(pCuName), packedShape(pPackedShape), ioMode(pIoMode) {};
};

struct ExtendedBufferDescriptor : public BufferDescriptor {
    ExtendedBufferDescriptor(const std::string& pKernelName, const std::string& pCuName, const shape_t& pPackedShape, const shape_t& pNormalShape, const shape_t& pFoldedShape, IO pIoMode)
        : BufferDescriptor(pKernelName, pCuName, pPackedShape, pIoMode), normalShape(pNormalShape), foldedShape(pFoldedShape) {};
    shape_t normalShape;
    shape_t foldedShape;
};

class FinnConfiguration {
    private:
    std::string xclbinPath;
    std::unordered_map<std::string, ExtendedBufferDescriptor> ebds;

    public:
    FinnConfiguration(/* std::string configJsonPath */) {
        // TODO(bwintermann): Fill with data / read from json file
    }
};


#endif  // TYPES_H