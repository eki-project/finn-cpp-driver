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

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
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
         * @brief Device descriptions, keyed by their xrtDeviceIndex
         *
         */
        std::map<unsigned int, DeviceWrapper> deviceWrappers;
        /**
         * @brief xrtDeviceIndex of the device that receives the input
         *
         */
        unsigned int inputDeviceIndex;
        /**
         * @brief xrtDeviceIndex of the device that produces the output
         *
         */
        unsigned int outputDeviceIndex;
        /**
         * @brief Position of the input kernel in the idmas of the input device
         *
         */
        unsigned int inputKernelIndex;
        /**
         * @brief Position of the output kernel in the odmas of the output device
         *
         */
        unsigned int outputKernelIndex;

        /**
         * @brief Get the device description with the given xrtDeviceIndex
         *
         * @param xrtDeviceIndex
         * @return const DeviceWrapper&
         * @throws std::runtime_error if no device wrapper has this xrtDeviceIndex
         */
        const DeviceWrapper& getDeviceWrapper(unsigned int xrtDeviceIndex) const {
            try {
                return deviceWrappers.at(xrtDeviceIndex);
            } catch (const std::out_of_range&) { throw std::runtime_error(std::format("No device wrapper with xrtDeviceIndex {} exists in the configuration!", xrtDeviceIndex)); }
        }

        /**
         * @brief Check that the input/output device and kernel indices refer to existing devices and DMAs
         *
         * @throws std::runtime_error if a device or kernel index does not exist
         */
        void validate() const {
            if (!deviceWrappers.contains(inputDeviceIndex)) {
                throw std::runtime_error(std::format("The input device index {} does not match the xrtDeviceIndex of any device in the configuration!", inputDeviceIndex));
            }
            if (!deviceWrappers.contains(outputDeviceIndex)) {
                throw std::runtime_error(std::format("The output device index {} does not match the xrtDeviceIndex of any device in the configuration!", outputDeviceIndex));
            }
            if (inputKernelIndex >= getDeviceWrapper(inputDeviceIndex).idmas.size()) {
                throw std::runtime_error(std::format("The input device {} has no IDMA of index {}!", inputDeviceIndex, inputKernelIndex));
            }
            if (outputKernelIndex >= getDeviceWrapper(outputDeviceIndex).odmas.size()) {
                throw std::runtime_error(std::format("The output device {} has no ODMA of index {}!", outputDeviceIndex, outputKernelIndex));
            }
        }
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
     * @brief Determine the input or output device (xrtDeviceIndex) of a config file.
     *
     * Uses the top-level entry `key` if the file has one. Otherwise the config must contain exactly one device, which is then both the input and the output device.
     *
     * @param configJson Parsed config file. In the legacy format this is the bare list of devices, which never contains `key`.
     * @param configPath Path of the config file, for error messages
     * @param key "inputDeviceIndex" or "outputDeviceIndex"
     * @param config Config with all deviceWrappers already parsed
     * @return unsigned int xrtDeviceIndex of the selected device
     * @throws std::runtime_error if the given index matches no device, or the index is missing although there are multiple devices
     */
    inline unsigned int resolveDeviceIndex(const json& configJson, const std::filesystem::path& configPath, const std::string& key, const Config& config) {
        if (configJson.contains(key)) {
            const auto xrtDeviceIndex = configJson.at(key).get<unsigned int>();
            if (!config.deviceWrappers.contains(xrtDeviceIndex)) {
                throw std::runtime_error(std::format("\"{}\" in config file {} is {}, but no device has this xrtDeviceIndex!", key, configPath.string(), xrtDeviceIndex));
            }
            return xrtDeviceIndex;
        }
        if (config.deviceWrappers.size() != 1) {
            throw std::runtime_error(std::format("Config file {} has {} devices, so \"{}\" must be given at the top level (next to the \"devices\" list)!", configPath.string(), config.deviceWrappers.size(), key));
        }
        // Single device: it is the only possible input/output device
        return config.deviceWrappers.begin()->first;
    }

    /**
     * @brief Determine the input or output kernel (position in idmas/odmas) on the selected device.
     *
     * Uses the top-level entry `key` if the file has one. Otherwise the device must have exactly one DMA of this kind, which is then selected.
     *
     * @param configJson Parsed config file. In the legacy format this is the bare list of devices, which never contains `key`.
     * @param configPath Path of the config file, for error messages
     * @param key "inputKernelIndex" or "outputKernelIndex"
     * @param device The selected input/output device
     * @param dmas The idmas (input) or odmas (output) of that device
     * @param dmaKind "IDMA" or "ODMA", for error messages
     * @return unsigned int Position of the selected kernel in dmas
     * @throws std::runtime_error if the device has no such DMA, the given index is out of range, or the index is missing although there are multiple DMAs
     */
    inline unsigned int resolveKernelIndex(const json& configJson, const std::filesystem::path& configPath, const std::string& key, const DeviceWrapper& device, const std::vector<std::shared_ptr<BufferDescriptor>>& dmas,
                                           const std::string& dmaKind) {
        if (dmas.empty()) {
            throw std::runtime_error(std::format("Device {} in config file {} has no {}!", device.xrtDeviceIndex, configPath.string(), dmaKind));
        }
        if (configJson.contains(key)) {
            const auto kernelIndex = configJson.at(key).get<unsigned int>();
            if (kernelIndex >= dmas.size()) {
                throw std::runtime_error(std::format("\"{}\" in config file {} is {}, but device {} only has {} {}(s)!", key, configPath.string(), kernelIndex, device.xrtDeviceIndex, dmas.size(), dmaKind));
            }
            return kernelIndex;
        }
        if (dmas.size() != 1) {
            throw std::runtime_error(std::format("Device {} in config file {} has {} {}s, so \"{}\" must be given at the top level (next to the \"devices\" list)!", device.xrtDeviceIndex, configPath.string(), dmas.size(), dmaKind, key));
        }
        // Single DMA: it is the only possible input/output kernel
        return 0;
    }

    /**
     * @brief Create a Config from a filepath by parsing it's JSON.
     *
     * The file is an object {"devices": [...], "inputDeviceIndex": ..., "outputDeviceIndex": ..., "inputKernelIndex": ..., "outputKernelIndex": ...}, or in the legacy format only the list of devices.
     * Device indices are xrtDeviceIndex values, kernel indices are positions in the idmas/odmas of the input/output device. An index may only be left out if there is exactly one candidate for it,
     * so the legacy format only works for a single device with a single IDMA and ODMA.
     *
     * @param configPath
     * @return Config
     * @throws std::runtime_error if the file is malformed, or an input/output index is ambiguous or does not exist
     */
    inline Config createConfigFromPath(const std::filesystem::path& configPath) {
        if (!std::filesystem::exists(configPath) || !std::filesystem::is_regular_file(configPath)) {
            throw std::filesystem::filesystem_error("File " + configPath.string() + " not found. Abort.", std::error_code());
        }
        std::ifstream file(configPath);
        const json dataJson = json::parse(file);
        const bool legacyFormat = dataJson.is_array();
        if (!legacyFormat && !(dataJson.is_object() && dataJson.contains("devices"))) {
            throw std::runtime_error(std::format("Config file {} must be either a list of devices or an object with a \"devices\" list!", configPath.string()));
        }

        Config config;
        for (const auto& fpgaDevice : legacyFormat ? dataJson : dataJson.at("devices")) {
            DeviceWrapper devWrap;
            from_json(fpgaDevice, devWrap);
            const unsigned int xrtIdx = devWrap.xrtDeviceIndex;
            if (!config.deviceWrappers.emplace(xrtIdx, std::move(devWrap)).second) {
                throw std::runtime_error(std::format("Duplicate xrtDeviceIndex {} in config file {}!", xrtIdx, configPath.string()));
            }
        }
        if (config.deviceWrappers.empty()) {
            throw std::runtime_error(std::format("Config file {} contains no devices!", configPath.string()));
        }

        // Select the input/output devices and kernels: given explicitly at the top level, or derived when there is exactly one candidate (see resolveDeviceIndex/resolveKernelIndex)
        config.inputDeviceIndex = resolveDeviceIndex(dataJson, configPath, "inputDeviceIndex", config);
        config.outputDeviceIndex = resolveDeviceIndex(dataJson, configPath, "outputDeviceIndex", config);

        const DeviceWrapper& inputDevice = config.getDeviceWrapper(config.inputDeviceIndex);
        const DeviceWrapper& outputDevice = config.getDeviceWrapper(config.outputDeviceIndex);
        config.inputKernelIndex = resolveKernelIndex(dataJson, configPath, "inputKernelIndex", inputDevice, inputDevice.idmas, "IDMA");
        config.outputKernelIndex = resolveKernelIndex(dataJson, configPath, "outputKernelIndex", outputDevice, outputDevice.odmas, "ODMA");
        return config;
    }

    /**
     * @brief Get the normal, folded and packed shapes for a specific device and dma
     *
     * @param conf Config from which the shapes should be read
     * @param xrtDeviceIndex xrt device index of the device
     * @param dma dma index
     * @return std::tuple<shape_t, shape_t, shape_t>
     */
    inline std::tuple<shape_t, shape_t, shape_t> getConfigShapes(const Config& conf, uint xrtDeviceIndex, uint dma) {
        const auto& idma = conf.getDeviceWrapper(xrtDeviceIndex).idmas.at(dma);
        auto myShapeNormal = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(idma)).normalShape;
        auto myShapeFolded = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(idma)).foldedShape;
        auto myShapePacked = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(idma)).packedShape;
        return {myShapeNormal, myShapeFolded, myShapePacked};
    }


}  // namespace Finn

#endif  // CONFIGURATIONSTRUCTS
