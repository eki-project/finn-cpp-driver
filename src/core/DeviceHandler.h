#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H
// #include <filesystem>
// #include <string>
// #include <unordered_map>
// #include <vector>

// // XRT
// #include "xrt.h"
// #include "xrt/xrt_bo.h"
// #include "xrt/xrt_device.h"
// #include "xrt/xrt_kernel.h"

// // FINN
// #include "../utils/Types.h"
// #include "DeviceBuffer.hpp"

#include <xrt/xrt_uuid.h>  // for uuid

#include <cstddef>        // for size_t
#include <cstdint>        // for uint8_t
#include <filesystem>     // for path
#include <iterator>       // for iterator_traits
#include <string>         // for string
#include <type_traits>    // for is_same
#include <unordered_map>  // for unordered_map
#include <utility>        // for shared_ptr
#include <vector>         // for vector

#include "../utils/Types.h"  // for shape_t
#include "DeviceBuffer.hpp"  // for DeviceInputBuffer, DeviceOutputBuffer
#include "xrt/xrt_device.h"  // for device
#include "../utils/Logger.h" // for logging


namespace Finn {
    /**
     * @brief Object of DeviceHandler is responsible to handle a programming of a Device and communication to it
     *
     */
    class DeviceHandler {
         private:
        logger_type& log = Logger::getLogger();
        xrt::device device;
        unsigned int xrtDeviceIndex;
        std::string xclbinPath;
        xrt::uuid uuid;
        std::unordered_map<std::string, DeviceInputBuffer<uint8_t>> inputBufferMap;
        std::unordered_map<std::string, DeviceOutputBuffer<uint8_t>> outputBufferMap;

         public:
        DeviceHandler(const DeviceWrapper&, unsigned int);
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


        void checkDeviceWrapper(const DeviceWrapper& devWrap); 

         protected:
        void initializeDevice();
        void loadXclbinSetUUID(); 
        void initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize); 

    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
