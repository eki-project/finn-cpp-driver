#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <filesystem>
#include <vector>

#include "DeviceHandler.h"

namespace Finn {

    struct DeviceWrapper {
        std::filesystem::path xclbin;
        std::string name;
        std::vector<std::string> idmas;
        std::vector<std::string> odmas;
    };


    class Accelerator {
         public:
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions);
        explicit Accelerator(const DeviceWrapper& deviceWrapper);
        Accelerator(Accelerator&&) = default;
        Accelerator(const Accelerator&) = default;
        Accelerator& operator=(Accelerator&&) = default;
        Accelerator& operator=(const Accelerator&) = default;
        ~Accelerator() = default;

         private:
        std::vector<DeviceHandler> devices;
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
