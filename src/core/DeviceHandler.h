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
    class DeviceHandler {
         public:
        DeviceHandler(const std::filesystem::path& xclbinPath, const std::string& name, std::size_t deviceIndex, const std::vector<std::string>& pInputNames, const std::vector<std::string>& pOutputNames);
        DeviceHandler(DeviceHandler&&) = default;
        DeviceHandler(const DeviceHandler&) = default;
        DeviceHandler& operator=(DeviceHandler&&) = default;
        DeviceHandler& operator=(const DeviceHandler&) = default;
        ~DeviceHandler() = default;

        void initializeDevice(std::size_t deviceIndex, const std::filesystem::path& xclbinPath, const std::vector<std::string>& inputKernelNames, const std::vector<std::string>& outputKernelNames);
        void initializeBufferObjects() const;

         private:
        std::string name;

        xrt::device device;
        xrt::uuid uuid;
        std::vector<xrt::kernel> inputKernels;
        std::vector<xrt::kernel> outputKernels;
    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
