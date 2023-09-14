#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H
#include <filesystem>
#include <string>
#include <vector>

// XRT
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Finn
#include "DeviceBuffer.hpp"

namespace Finn {
    /**
     * @brief A small storage struct to manage the description of Buffers
     *
     */
    struct BufferDescriptor {
        std::string kernelName;
        shape_t elementShape;
    };
    /**
     * @brief Object of DeviceHandler is responsible to handle a programming of a Device and communication to it
     *
     */
    class DeviceHandler {
         public:
        /**
         * @brief Construct a new Device Handler object
         *
         * @param xclbinPath Path to xclbin used for programming
         * @param name Name of device used in logs
         * @param deviceIndex Index of device
         * @param inputBufDescr List of descriptions of all input buffers
         * @param outputBufDescr List of descriptions of all output buffers
         * @param hostBufferSize Size of input/output buffers in elements
         */
        DeviceHandler(const std::filesystem::path& xclbinPath, const std::string& pName, std::size_t deviceIndex, const std::vector<BufferDescriptor>& inputBufDescr, const std::vector<BufferDescriptor>& outputBufDescr,
                      std::size_t hostBufferSize = 64);
        /**
         * @brief Default move constructor
         *
         */
        DeviceHandler(DeviceHandler&&) = default;
        /**
         * @brief Deleted copy constructor
         *
         */
        DeviceHandler(const DeviceHandler&) = delete;
        /**
         * @brief Default move assignment operator
         *
         * @return DeviceHandler&
         */
        DeviceHandler& operator=(DeviceHandler&&) = default;
        /**
         * @brief Deleted copy assignment operator
         *
         * @return DeviceHandler&
         */
        DeviceHandler& operator=(const DeviceHandler&) = default;
        /**
         * @brief Destroy the Device Handler object
         *
         */
        ~DeviceHandler() = default;

         protected:
        /**
         * @brief Program and initialize the managed device
         *
         * @param deviceIndex Index of device
         * @param xclbinPath Path to file used for programming
         */
        void initializeDevice(std::size_t deviceIndex, const std::filesystem::path& xclbinPath);
        /**
         * @brief Initializes input and output buffers of the loaded design
         *
         * @param inputBufDescr Descriptions of buffer configurations for the input buffers
         * @param outputBufDescr Descriptions of buffer configurations for the output buffers
         * @param hostBufferSize Size of host buffers in number of elements
         */
        void initializeBufferObjects(const std::vector<BufferDescriptor>& inputBufDescr, const std::vector<BufferDescriptor>& outputBufDescr, std::size_t hostBufferSize);

         private:
        /**
         * @brief Name of device used in log messages
         *
         */
        std::string name;

        /**
         * @brief XRT device managed by the object
         *
         */
        xrt::device device;
        /**
         * @brief uuid of programmed device
         *
         */
        xrt::uuid uuid;
        /**
         * @brief Names of idmas
         *
         */
        std::vector<xrt::kernel> inputKernels;
        std::vector<DeviceInputBuffer<uint8_t>> inputBuffer;
        /**
         * @brief Names of odmas
         *
         */
        std::vector<xrt::kernel> outputKernels;
        std::vector<DeviceOutputBuffer<uint8_t>> outputBuffer;
    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
