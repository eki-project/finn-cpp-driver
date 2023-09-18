#ifndef TYPES_H
#define TYPES_H

#include <cctype>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace Finn {
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
        std::string kernelName;
        shape_t packedShape;

        BufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape) : kernelName(pKernelName), packedShape(pPackedShape){};
    };

    struct ExtendedBufferDescriptor : public BufferDescriptor {
        ExtendedBufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape, const shape_t& pNormalShape, const shape_t& pFoldedShape)
            : BufferDescriptor(pKernelName, pPackedShape), normalShape(pNormalShape), foldedShape(pFoldedShape){};
        shape_t normalShape;
        shape_t foldedShape;
    };

    // NOLINTNEXTLINE
    void inline to_json(nlohmann::json& j, const ExtendedBufferDescriptor& ebd) { j = nlohmann::json{{"kernelName", ebd.kernelName}, {"packedShape", ebd.packedShape}, {"normalShape", ebd.normalShape}, {"foldedShape", ebd.foldedShape}}; }

    // NOLINTNEXTLINE
    void inline from_json(const nlohmann::json& j, ExtendedBufferDescriptor& ebd) {
        j.at("kernelName").get_to(ebd.kernelName);
        j.at("packedShape").get_to(ebd.packedShape);
        j.at("normalShape").get_to(ebd.normalShape);
        j.at("foldedShape").get_to(ebd.foldedShape);
    }

    /**
     * @brief Helper struct to structure input data for DeviceHandler creation
     *
     */
    struct DeviceWrapper {
        std::filesystem::path xclbin;
        std::string name;
        std::vector<BufferDescriptor> idmas;
        std::vector<BufferDescriptor> odmas;
    };
}  // namespace Finn


#endif  // TYPES_H