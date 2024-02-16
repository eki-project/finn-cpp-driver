/**
 * @file ConfigurationStructs.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides functionality to (de)serialize configuration JSON files at runtime
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef CONFIGURATIONSTRUCTS
#define CONFIGURATIONSTRUCTS

#include <FINNCppDriver/utils/Types.h>

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Abrievation of nlohmann::json
 *
 */
using json = nlohmann::json;

namespace nlohmann {
    /**
     * @brief Implements adl lookup for shared pointers
     *
     * @tparam T Type of ptr contained in shared ptr
     */
    template<typename T>
    struct adl_serializer<std::shared_ptr<T>> {
        /**
         * @brief Converts a shared ptr to json
         *
         * @param j json output
         * @param opt pointer input
         */
        // NOLINTNEXTLINE
        static void to_json(json& j, const std::shared_ptr<T>& opt) {
            if (opt) {
                j = *opt;
            } else {
                j = nullptr;
            }
        }

        /**
         * @brief Converts json to a shared ptr
         *
         * @param j json input
         * @param opt shared ptr input
         */
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


namespace Finn {
    /**
     * @brief A small storage struct to manage the description of Buffers
     *
     */
    struct BufferDescriptor : public std::enable_shared_from_this<BufferDescriptor> {
        /**
         * @brief Kernel Name (e.g. vadd:{inst0} / idma0:{inst0})
         *
         */
        std::string kernelName;
        /**
         * @brief This assumes that the values are in uint8_ts (instead of the Finn Datatypes)
         *
         */
        shape_t packedShape;

        // TODO(bwintermann): Currently unused, reserved for multi-fpga usage
        /**
         * @brief Currently unused, reserved for multi-fpga usage
         *
         */
        unsigned int slrIndex = 0;

        /**
         * @brief Construct a new Buffer Descriptor object
         *
         */
        BufferDescriptor() = default;
        /**
         * @brief Construct a new Buffer Descriptor object
         *
         * @param pKernelName Name of kernel
         * @param pPackedShape Input shape of kernel
         */
        BufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape) : kernelName(pKernelName), packedShape(pPackedShape){};
        /**
         * @brief Construct a new Buffer Descriptor object (Move constructor)
         *
         */
        BufferDescriptor(BufferDescriptor&&) = default;
        /**
         * @brief Construct a new Buffer Descriptor object (Copy constructor)
         *
         */
        BufferDescriptor(const BufferDescriptor&) = default;
        /**
         * @brief Move assignment operator
         *
         * @return BufferDescriptor&
         */
        BufferDescriptor& operator=(BufferDescriptor&&) = default;
        /**
         * @brief Copy assignment operator
         *
         * @return BufferDescriptor&
         */
        BufferDescriptor& operator=(const BufferDescriptor&) = default;

        /**
         * @brief Destroy the Buffer Descriptor object
         *
         */
        virtual ~BufferDescriptor() = default;  // Needed for dynamic cast
    };

    /**
     * @brief Extended version of the buffer descriptor which includes the normal and folded shapes
     *
     */
    struct ExtendedBufferDescriptor : public BufferDescriptor {
        /**
         * @brief Construct a new Extended Buffer Descriptor object
         *
         */
        ExtendedBufferDescriptor() = default;
        /**
         * @brief Construct a new Extended Buffer Descriptor object
         *
         * @param pKernelName see @see BufferDescriptor
         * @param pPackedShape see @see BufferDescriptor
         * @param pNormalShape Input shape of neural network
         * @param pFoldedShape Internal reshaped form
         */
        ExtendedBufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape, const shape_t& pNormalShape, const shape_t& pFoldedShape)
            : BufferDescriptor(pKernelName, pPackedShape), normalShape(pNormalShape), foldedShape(pFoldedShape){};

        /**
         * @brief Input shape of neural network
         *
         */
        shape_t normalShape;
        /**
         * @brief Internal reshaped form
         *
         */
        shape_t foldedShape;
    };

    /**
     * @brief Helper struct to structure input data for DeviceHandler creation
     *
     */
    struct DeviceWrapper {
        /**
         * @brief Path to xclbin
         *
         */
        std::filesystem::path xclbin;
        /**
         * @brief XRT device index assigned to this device
         *
         */
        unsigned int xrtDeviceIndex = 0;
        /**
         * @brief List of idma descriptions for this device
         *
         */
        std::vector<std::shared_ptr<BufferDescriptor>> idmas;
        /**
         * @brief List of odma descriptions for this device
         *
         */
        std::vector<std::shared_ptr<BufferDescriptor>> odmas;

        /**
         * @brief Construct a new Device Wrapper object
         *
         * @param pXclbin Path to xclbin
         * @param pXrtDeviceIndex XRT device index assigned to this device
         * @param pIdmas List of idma descriptions for this device
         * @param pOdmas List of odma descriptions for this device
         */
        DeviceWrapper(const std::filesystem::path& pXclbin, const unsigned int pXrtDeviceIndex, const std::vector<std::shared_ptr<BufferDescriptor>>& pIdmas, const std::vector<std::shared_ptr<BufferDescriptor>>& pOdmas)
            : xclbin(pXclbin), xrtDeviceIndex(pXrtDeviceIndex), idmas(pIdmas), odmas(pOdmas){};

        /**
         * @brief Construct a new Device Wrapper object
         *
         */
        DeviceWrapper() = default;
    };

    /**
     * @brief Encompassing struct that represents an entire configuration
     *
     */
    struct Config {
        /**
         * @brief List of device descriptions
         *
         */
        std::vector<DeviceWrapper> deviceWrappers;
    };


    /***** JSON CONVERSION FUNCTIONS ******/
    /**
     * @brief JSON -> ExtendedBufferDescriptor
     *
     * @param j
     * @param ebd
     */
    // NOLINTNEXTLINE
    void inline to_json(json& j, const ExtendedBufferDescriptor& ebd) { j = json{{"kernelName", ebd.kernelName}, {"packedShape", ebd.packedShape}, {"normalShape", ebd.normalShape}, {"foldedShape", ebd.foldedShape}}; }

    /**
     * @brief ExtendedBufferDescriptor -> JSON
     *
     * @param j
     * @param ebd
     */
    // NOLINTNEXTLINE
    void inline from_json(const json& j, ExtendedBufferDescriptor& ebd) {
        j.at("kernelName").get_to(ebd.kernelName);
        j.at("packedShape").get_to(ebd.packedShape);
        j.at("normalShape").get_to(ebd.normalShape);
        j.at("foldedShape").get_to(ebd.foldedShape);
    }

    /**
     * @brief JSON -> DeviceWrapper
     *
     * @param j
     * @param devWrap
     */
    // NOLINTNEXTLINE
    void inline from_json(const json& j, DeviceWrapper& devWrap) {
        j.at("xclbinPath").get_to(devWrap.xclbin);
        j.at("xrtDeviceIndex").get_to(devWrap.xrtDeviceIndex);
        auto vec = j.at("idmas").get<std::vector<std::shared_ptr<ExtendedBufferDescriptor>>>();
        devWrap.idmas = std::vector<std::shared_ptr<BufferDescriptor>>(vec.begin(), vec.end());
        vec = j.at("odmas").get<std::vector<std::shared_ptr<ExtendedBufferDescriptor>>>();
        devWrap.odmas = std::vector<std::shared_ptr<BufferDescriptor>>(vec.begin(), vec.end());
    }

    /**
     * @brief Create a Config from a filepath by parsing it's JSON.
     *
     * @param configPath
     * @return Config
     */
    inline Config createConfigFromPath(const std::filesystem::path& configPath) {
        if (!std::filesystem::exists(configPath) || !std::filesystem::is_regular_file(configPath)) {
            throw std::filesystem::filesystem_error("File " + configPath.string() + " not found. Abort.", std::error_code());
        }
        std::ifstream file(configPath);
        json dataJson = json::parse(file);
        Config config;
        for (auto& fpgaDevice : dataJson) {
            DeviceWrapper devWrap;
            from_json(fpgaDevice, devWrap);
            config.deviceWrappers.emplace_back(devWrap);
        }
        return config;
    }

    /**
     * @brief Get the normal, folded and packed shapes for a specific device and dma
     *
     * @param conf Config from which the shapes should be read
     * @param device device index
     * @param dma dma index
     * @return std::tuple<shape_t, shape_t, shape_t>
     */
    inline std::tuple<shape_t, shape_t, shape_t> getConfigShapes(const Config& conf, uint device = 0, uint dma = 0) {
        auto myShapeNormal = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers.at(device).idmas.at(dma))).normalShape;
        auto myShapeFolded = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers[device].idmas[dma])).foldedShape;
        auto myShapePacked = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers[device].idmas[dma])).packedShape;
        return {myShapeNormal, myShapeFolded, myShapePacked};
    }


}  // namespace Finn

#endif  // CONFIGURATIONSTRUCTS
