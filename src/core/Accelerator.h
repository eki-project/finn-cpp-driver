#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <cinttypes>   // for uint8_t
#include <filesystem>  // for path
#include <string>      // for string
#include <vector>      // for vector

#include "DeviceHandler.h"  // for BufferDescriptor, DeviceHandler


namespace Finn {
    // Fwd declarations
    // class DeviceHandler;
    // struct BufferDescriptor;


    /**
     * @brief The Accelerator class wraps one or more Devices into a single Accelerator
     *
     */
    class Accelerator {
         private:
        std::vector<DeviceHandler> devices;

         public:
        Accelerator() = default;
        /**
         * @brief Construct a new Accelerator object using a list of DeviceWrappers
         *
         * @param deviceDefinitions Vector of @ref DeviceWrapper
         */
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, unsigned int hostBufferSize);
        Accelerator(Accelerator&&) = default;
        Accelerator(const Accelerator&) = delete;
        Accelerator& operator=(Accelerator&&) = default;
        Accelerator& operator=(const Accelerator&) = delete;
        ~Accelerator() = default;
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
