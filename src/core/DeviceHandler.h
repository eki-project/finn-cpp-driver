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

namespace Finn {
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
         * @param inputNames Names of idmas
         * @param outputNames Names of odmas
         */
        DeviceHandler(const std::filesystem::path& xclbinPath, const std::string& name, std::size_t deviceIndex, const std::vector<std::string>& inputNames, const std::vector<std::string>& outputNames);
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

        /**
         * @brief Program and initialize the managed device
         *
         * @param deviceIndex Index of device
         * @param xclbinPath Path to file used for programming
         * @param inputKernelNames Names of idma
         * @param outputKernelNames Names of odma
         */
        void initializeDevice(std::size_t deviceIndex, const std::filesystem::path& xclbinPath, const std::vector<std::string>& inputKernelNames, const std::vector<std::string>& outputKernelNames);
        void initializeBufferObjects() const;

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
        /**
         * @brief Names of odmas
         *
         */
        std::vector<xrt::kernel> outputKernels;
    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
