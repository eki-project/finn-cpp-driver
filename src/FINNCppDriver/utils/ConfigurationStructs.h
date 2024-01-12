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


namespace Finn {
    /**
     * @brief A small storage struct to manage the description of Buffers
     *
     */
    struct BufferDescriptor : public std::enable_shared_from_this<BufferDescriptor> {
        std::string kernelName;  // Kernel Name (e.g. vadd:{inst0} / idma0:{inst0})
        shape_t packedShape;     // This assumes that the values are in uint8_ts (instead of the Finn Datatypes)

        // TODO(bwintermann): Currently unused, reserved for multi-fpga usage
        unsigned int slrIndex = 0;

        BufferDescriptor() = default;
        BufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape) : kernelName(pKernelName), packedShape(pPackedShape){};
        BufferDescriptor(BufferDescriptor&&) = default;
        BufferDescriptor(const BufferDescriptor&) = default;
        BufferDescriptor& operator=(BufferDescriptor&&) = default;
        BufferDescriptor& operator=(const BufferDescriptor&) = default;
        virtual ~BufferDescriptor() = default;  // Needed for dynamic cast
    };

    /**
     * @brief Extended version of the buffer descriptor which includes the normal and folded shapes
     *
     */
    struct ExtendedBufferDescriptor : public BufferDescriptor {
        ExtendedBufferDescriptor() = default;
        ExtendedBufferDescriptor(const std::string& pKernelName, const shape_t& pPackedShape, const shape_t& pNormalShape, const shape_t& pFoldedShape)
            : BufferDescriptor(pKernelName, pPackedShape), normalShape(pNormalShape), foldedShape(pFoldedShape){};
        shape_t normalShape;
        shape_t foldedShape;
    };

    /**
     * @brief Helper struct to structure input data for DeviceHandler creation
     *
     */
    struct DeviceWrapper {
        std::filesystem::path xclbin;
        unsigned int xrtDeviceIndex = 0;
        std::vector<std::shared_ptr<BufferDescriptor>> idmas;
        std::vector<std::shared_ptr<BufferDescriptor>> odmas;

        DeviceWrapper(const std::filesystem::path& pXclbin, const unsigned int pXrtDeviceIndex, const std::vector<std::shared_ptr<BufferDescriptor>>& pIdmas, const std::vector<std::shared_ptr<BufferDescriptor>>& pOdmas)
            : xclbin(pXclbin), xrtDeviceIndex(pXrtDeviceIndex), idmas(pIdmas), odmas(pOdmas){};
        DeviceWrapper() = default;
    };

    /**
     * @brief Encompassing struct that represents an entire configuration
     *
     */
    struct Config {
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

    inline std::tuple<shape_t, shape_t, shape_t> getConfigShapes(const Config& conf, uint device = 0, uint dma = 0) {
        auto myShapeNormal = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers.at(device).idmas.at(dma))).normalShape;
        auto myShapeFolded = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers[device].idmas[dma])).foldedShape;
        auto myShapePacked = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(conf.deviceWrappers[device].idmas[dma])).packedShape;
        return {myShapeNormal, myShapeFolded, myShapePacked};
    }


}  // namespace Finn

#endif  // CONFIGURATIONSTRUCTS
