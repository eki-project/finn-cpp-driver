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
#include <vector>         // for vector

#include "../utils/Types.h"  // for shape_t
#include "DeviceBuffer.hpp"  // for DeviceInputBuffer, DeviceOutputBuffer
#include "xrt/xrt_device.h"  // for device


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

        /**
         * @brief Write the data provided using the begin and end iterators to the device input buffer. This version of the write function works iff the FPGA has only one input!
         * @attention Dev: At the moment this member function performs an additional copy of the data and should therefore not be used in realtime situations.
         *
         * @tparam InputIt Type of the input iterator (In general this template parameter is automatically infered)
         * @param first Iterator to the first element to be transfered
         * @param last Iterator to the last element to be transfered
         * @return true Write to Buffer was successful
         * @return false Write to Buffer did not succeed
         */
        template<class InputIt>
        bool write(InputIt first, InputIt last) {
            static_assert(std::is_same<typename std::iterator_traits<InputIt>::value_type, uint8_t>::value);
            // TODO(linusjun): rewrite to just forward iterators to DeviceBuffer
            std::vector<uint8_t> vec(first, last);
            return write(vec);
        }

        /**
         * @brief Write the data provided using the vector reference to the device input buffer. This version of the write function works iff the FPGA has only one input!
         * @attention Dev: This member function is real time safe, iff the initialization of the buffer object does not report hash conflicts and the underlying device buffer is realtime safe
         *
         * @param inputVec Vector of data to be transfered
         * @return true Write to Buffer was successful
         * @return false Write to Buffer did not succeed
         */
        bool write(const std::vector<uint8_t>& inputVec);

        /**
         * @brief Write the data provided using the begin and end iterators to the device input buffer.
         * @attention Dev: At the moment this member function performs an additional copy of the data and should therefore not be used in realtime situations.
         *
         * @tparam InputIt Type of the input iterator (In general this template parameter is automatically infered)
         * @param first Iterator to the first element to be transfered
         * @param last Iterator to the last element to be transfered
         * @param inputBufferName Name of the buffer to which the data should be written
         * @return true Write to Buffer was successful
         * @return false Write to Buffer did not succeed
         */
        template<class InputIt>
        bool write(InputIt first, InputIt last, std::string& inputBufferName) {
            static_assert(std::is_same<typename std::iterator_traits<InputIt>::value_type, uint8_t>::value);
            // TODO(linusjun): rewrite to just forward iterators to DeviceBuffer
            std::vector<uint8_t> vec(first, last);
            return write(vec, inputBufferName);
        }

        /**
         * @brief Write the data provided using the vector reference to the device input buffer.
         *
         * @param inputVec Write the data provided using the vector reference to the device input buffer.
         * @param inputBufferName Name of the buffer to which the data should be written
         * @return true Write to Buffer was successful
         * @return false Write to Buffer did not succeed
         */
        bool write(const std::vector<uint8_t>& inputVec, const std::string& inputBufferName);

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
         * @brief InputBuffers used for data input
         *
         */
        std::unordered_map<std::string, DeviceInputBuffer<uint8_t>> inputBufferMap;
        /**
         * @brief OutputBuffers used for data retrieval
         *
         */
        std::unordered_map<std::string, DeviceOutputBuffer<uint8_t>> outputBufferMap;
    };
}  // namespace Finn

#endif  // !DEVICEHANDLER_H
