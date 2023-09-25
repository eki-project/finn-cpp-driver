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
#include "../utils/ConfigurationStructs.h"
#include "ert.h"

namespace Finn {
    /**
     * @brief Object of DeviceHandler is responsible to handle a programming of a Device and communication to it
     *
     */
    class DeviceHandler {
         private:
        /**
         * @brief The xrt device itself 
         * 
         */
        xrt::device device;
        
        /**
         * @brief The local device index. This is used to create the xrt::device. TODO(bwintermann): Fix for multiple nodes (fpgas are always only numbered 0-2, not 0-2, 3-5, etc.)
         * 
         */
        unsigned int xrtDeviceIndex;

        /**
         * @brief Path to this devices bitstream file. TODO(linusjun,bwintermann): Change to std::fs::path 
         * 
         */
        std::string xclbinPath;
        xrt::uuid uuid;

        /**
         * @brief Map containing all DeviceInputBuffers for this device 
         * 
         */
        std::unordered_map<std::string, DeviceInputBuffer<uint8_t>> inputBufferMap;
        
        /**
         * @brief Map containing all DeviceOutputBuffers for this device 
         * 
         */
        std::unordered_map<std::string, DeviceOutputBuffer<uint8_t>> outputBufferMap;



         public:
        DeviceHandler(const DeviceWrapper& devWrap, unsigned int hostBufferSize);
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
         * @brief Check if a correct DeviceWrapper configuration was given 
         * 
         * @param devWrap 
         */
        static void checkDeviceWrapper(const DeviceWrapper& devWrap); 

        /**
         * @brief Get the Device Index of this device handler
         * 
         * @return unsigned int 
         */
        unsigned int getDeviceIndex() const;

        /**
         * @brief Return a reference to the actual xrt::device object used
         * 
         * @return xrt::device& 
         */
        xrt::device& getDevice();

        /**
         * @brief Store the given vector data in the corresponding buffer. 
         * 
         * @param data The data to store 
         * @param inputBufferKernelName The kernelName which specifies the buffer
         * @return true 
         * @return false 
         */
        bool store(const std::vector<uint8_t>& data, const std::string& inputBufferKernelName);

        /**
         * @brief Same as store, but without performing a check whether the kernel exists before accessing 
         * 
         * @param data 
         * @param inputBufferKernelName 
         * @return true 
         * @return false 
         */
        bool storeUnchecked(const std::vector<uint8_t>& data, const std::string& inputBufferKernelName);

        /**
         * @brief Run the kernel of the given name. Returns true if successfull, returns false if no valid data to write was found 
         * 
         * @param inputBufferKernelName 
         * @return true 
         * @return false 
         */
        bool run(const std::string& inputBufferKernelName);

        /**
         * @brief Read from the output buffer on the host. This does NOT execute the output kernel 
         * 
         * @param outputBufferKernelName 
         * @param forceArchive If true, the data gets copied from the buffer to the long term storage immediately. If false, the newest read data might not actually be returned by this function
         * @param samples Number of samples to read
         * @return std::vector<std::vector<uint8_t>> 
         */
        std::vector<std::vector<uint8_t>> retrieveResults(const std::string& outputBufferKernelName, bool forceArchival);

        /**
         * @brief Execute the output kernel and return it's result. If a run fails, the function returns early. 
         * 
         * @param outputBufferKernelName 
         * @param samples 
         * @return ert_cmd_state 
         */
        ert_cmd_state read(const std::string& outputBufferKernelName, unsigned int samples);

        /**
         * @brief Return the buffer sizes 
         * 
         * @param ss 
         * @param bufferName 
         * @return size_t 
         */
        size_t size(SIZE_SPECIFIER ss, const std::string& bufferName);

        /**
         * @brief Return whether there is a kernel with the given name in this device 
         * 
         * @param kernelBufferName 
         * @param ioMode
         * @return true 
         * @return false 
         */
        bool containsBuffer(const std::string& kernelBufferName, IO ioMode);


         protected:
        /**
         * @brief Initialize the device by it's given xrtDeviceIndex, initializing the "device" member variable 
         * 
         */
        void initializeDevice();
        
        /**
         * @brief Loading the given xclbin by it's path. Sets the "uuid" member variable 
         * 
         */
        void loadXclbinSetUUID(); 

        /**
         * @brief Create DeviceBuffers for every idma/odma in the DeviceWrapper. 
         * 
         * @param devWrap 
         * @param hostBufferSize How many multiples of one sample should be store-able in the buffer
         */
        void initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize); 
    


    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
