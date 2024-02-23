/**
 * @file DeviceHandler.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Encapsulates and manages a complete FPGA device
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H

#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/FinnUtils.h>  // for logAndError
#include <FINNCppDriver/utils/Types.h>      // for shape_t
#include <stddef.h>                         // for size_t

#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <cstdint>        // for uint8_t
#include <iterator>       // for iterator_traits
#include <memory>         // for shared_ptr
#include <span>           // for span
#include <stdexcept>      // for runtime_error
#include <string>         // for string
#include <type_traits>    // for is_same
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector

#include "ert.h"
#include "xrt/xrt_device.h"  // for device
#include "xrt/xrt_uuid.h"    // for uuid

namespace Finn {
    class UncheckedStore;
    /**
     * @brief Object of DeviceHandler is responsible to handle a programming of a Device and communication to it
     *
     */
    class DeviceHandler {
         private:
        friend UncheckedStore;

        /**
         * @brief Switch to decide if inference happens synchronous or asynchronous
         *
         */
        bool synchronousInference;

        /**
         * @brief Device information needed to initialize device
         *
         */
        DeviceWrapper devInformation;

        /**
         * @brief The current batch size
         *
         */
        uint batchsize = 1;

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
        std::unordered_map<std::string, std::shared_ptr<DeviceInputBuffer<uint8_t>>> inputBufferMap;

        /**
         * @brief Map containing all DeviceOutputBuffers for this device
         *
         */
        std::unordered_map<std::string, std::shared_ptr<DeviceOutputBuffer<uint8_t>>> outputBufferMap;


         public:
        /**
         * @brief Construct a new Device Handler object
         *
         * @param devWrap
         * @param synchronousInference
         * @param hostBufferSize
         */
        explicit DeviceHandler(const DeviceWrapper& devWrap, bool synchronousInference, unsigned int hostBufferSize);
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
        DeviceHandler& operator=(const DeviceHandler&) = delete;
        /**
         * @brief Destroy the Device Handler object
         *
         */
        ~DeviceHandler() = default;

        /**
         * @brief Sets the input batch size. Needs to reinitialize all buffers!
         *
         * @param batchsize
         */
        void setBatchSize(uint batchsize);

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
         * @brief Get the Input Buffer Map
         *
         * @return std::unordered_map<std::string, std::shared_ptr<DeviceInputBuffer<uint8_t>>>&
         */
        std::unordered_map<std::string, std::shared_ptr<DeviceInputBuffer<uint8_t>>>& getInputBufferMap();

        /**
         * @brief Get the Output Buffer Map
         *
         * @return std::unordered_map<std::string, std::shared_ptr<DeviceOutputBuffer<uint8_t>>>&
         */
        std::unordered_map<std::string, std::shared_ptr<DeviceOutputBuffer<uint8_t>>>& getOutputBufferMap();


        /**
         * @brief Run the kernel of the given name. Returns true if successful, returns false if no valid data to write was found
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
         * @param forceArchival If true, the data gets copied from the buffer to the long term storage immediately. If false, the newest read data might not actually be returned by this function
         * @return Finn::vector<uint8_t>
         */
        Finn::vector<uint8_t> retrieveResults(const std::string& outputBufferKernelName, bool forceArchival);

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

        /**
         * @brief Stores an input into the Device Buffer
         *
         * @tparam IteratorType
         * @param first Iterator to first element of input
         * @param last Iterator to end of input
         * @param inputBufferKernelName identifier of the buffer kernel
         * @return true success
         * @return false failure
         */
        template<typename IteratorType>
        bool store(IteratorType first, IteratorType last, const std::string& inputBufferKernelName) {
            if (!inputBufferMap.contains(inputBufferKernelName)) {
                FinnUtils::logAndError<std::runtime_error>("Tried accessing kernel/buffer with name " + inputBufferKernelName + " but this kernel / buffer does not exist!");
            }
            return inputBufferMap.at(inputBufferKernelName)->store(first, last);
        }

        /**
         * @brief Get an input buffer from this device based on its name
         *
         * @param name
         * @return std::shared_ptr<DeviceInputBuffer<uint8_t>>&
         */
        std::shared_ptr<DeviceInputBuffer<uint8_t>>& getInputBuffer(const std::string& name);

        /**
         * @brief Get the Output Buffer from this device by its name
         *
         * @param name
         * @return std::shared_ptr<DeviceOutputBuffer<uint8_t>>&
         */
        std::shared_ptr<DeviceOutputBuffer<uint8_t>>& getOutputBuffer(const std::string& name);


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
         * @param synchronousInference
         */
        void initializeBufferObjects(const DeviceWrapper& devWrap, unsigned int hostBufferSize, bool pSynchronousInference);

         private:
        /**
         * @brief A logger prefix to determine the source of a log write
         *
         * @return std::string
         */
        static std::string loggerPrefix();

        /**
         * @brief Store the provided data into the DeviceBuffer
         *
         * @tparam IteratorType
         * @param first
         * @param last
         * @param inputBufferKernelName Name of the kernel to be used when transfering data to FPGA
         * @return true
         * @return false
         */
        template<typename IteratorType>
        bool storeUnchecked(IteratorType first, IteratorType last, const std::string& inputBufferKernelName) {
            static_assert(std::is_same<typename std::iterator_traits<IteratorType>::value_type, uint8_t>::value);
            return inputBufferMap.at(inputBufferKernelName)->store(std::span<const uint8_t>(first, last));
        }


#ifndef NDEBUG
        /**
         * @brief Check if the input and output buffer maps are collision free (i.e. have O(1) access). Also logs.
         *
         * @return true
         * @return false
         */
        bool isBufferMapCollisionFree();
#endif
    };

    /**
     * @brief Functor for faster store operations
     *
     */
    class UncheckedStore {
        DeviceHandler& dev;
        std::string inputBufferName;

         public:
        /**
         * @brief Dummy constructor, this one should never be used
         *
         */
        UncheckedStore(DeviceHandler& pDev, const std::string& pInputBufferName) : dev(pDev), inputBufferName(pInputBufferName) {}

        /**
         * @brief Stores the data vector into a device buffer
         *
         * @param data input data
         * @return true success
         * @return false failure
         */
        bool operator()(const Finn::vector<uint8_t>& data) { return dev.storeUnchecked(data.begin(), data.end(), inputBufferName); }

        /**
         * @brief Stores the data vector into a device buffer
         *
         * @tparam IteratorType
         * @param first Iterator to first element to be stored
         * @param last Iterator to end of input
         * @return true success
         * @return false failure
         */
        template<typename IteratorType>
        bool operator()(IteratorType first, IteratorType last) {
            return dev.storeUnchecked(first, last, inputBufferName);
        }
    };

}  // namespace Finn

#endif  // !DEVICEHANDLER_H
