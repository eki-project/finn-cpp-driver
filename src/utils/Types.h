#ifndef TYPES_H
#define TYPES_H

#include <cctype>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
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

using json = nlohmann::json;

namespace nlohmann {
    template<typename T>
    struct adl_serializer<std::shared_ptr<T>> {
        // NOLINTNEXTLINE
        static void to_json(json& j, const std::shared_ptr<T>& opt) {
            if (opt) {
                j = *opt;
            } else {
                j = nullptr;
            }
        }
        // NOLINTNEXTLINE
        static void from_json(const json& j, std::shared_ptr<T>& opt) {
            if (j.is_null()) {
                opt = nullptr;
            } else {
                opt.reset(new T(j.get<T>()));
            }
        }
    };
}  // namespace nlohmann

/**
 * @brief A small storage struct to manage the description of Buffers
 *
 */
struct BufferDescriptor : public std::enable_shared_from_this<BufferDescriptor> {
    std::string kernelName;  // Kernel Name (e.g. vadd:{inst0} / idma0:{inst0})
    shape_t packedShape;

    // TODO(bwintermann): Currently unused, reserved for multi-fpga usage
    unsigned int slrIndex = 0;

    BufferDescriptor() = default;
    BufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape) : kernelName(pKernelName), packedShape(pPackedShape){};
    virtual ~BufferDescriptor() = default;  // Needed for dynamic cast
};

struct ExtendedBufferDescriptor : public BufferDescriptor {
    ExtendedBufferDescriptor() = default;
    ExtendedBufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape, const shape_t& pNormalShape, const shape_t& pFoldedShape)
        : BufferDescriptor(pKernelName, pPackedShape), normalShape(pNormalShape), foldedShape(pFoldedShape){};
    shape_t normalShape;
    shape_t foldedShape;
};

// NOLINTNEXTLINE
void inline to_json(json& j, const ExtendedBufferDescriptor& ebd) { j = json{{"kernelName", ebd.kernelName}, {"packedShape", ebd.packedShape}, {"normalShape", ebd.normalShape}, {"foldedShape", ebd.foldedShape}}; }

// NOLINTNEXTLINE
void inline from_json(const json& j, ExtendedBufferDescriptor& ebd) {
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
    unsigned int fpgaIndex;
    std::vector<std::shared_ptr<BufferDescriptor>> idmas;
    std::vector<std::shared_ptr<BufferDescriptor>> odmas;
};

// NOLINTNEXTLINE
void inline from_json(const json& j, DeviceWrapper& devWrap) {
    j.at("xclbinPath").get_to(devWrap.xclbin);
    j.at("name").get_to(devWrap.name);
    j.at("fpgaIndex").get_to(devWrap.fpgaIndex);
    auto vec = j.at("idmas").get<std::vector<std::shared_ptr<ExtendedBufferDescriptor>>>();
    devWrap.idmas = std::vector<std::shared_ptr<BufferDescriptor>>(vec.begin(), vec.end());
    vec = j.at("odmas").get<std::vector<std::shared_ptr<ExtendedBufferDescriptor>>>();
    devWrap.odmas = std::vector<std::shared_ptr<BufferDescriptor>>(vec.begin(), vec.end());
}


#endif  // TYPES_H