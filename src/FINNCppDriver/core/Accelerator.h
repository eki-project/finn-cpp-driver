/**
 * @file Accelerator.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Implements a wrapper to hide away details of FPGA implementation
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <FINNCppDriver/core/DeviceHandler.h>  // for DeviceHandler, Uncheck...
#include <FINNCppDriver/utils/Types.h>         // for vector, SIZE_SPECIFIER

#include <cinttypes>  // for uint8_t
#include <cstddef>    // for size_t
#include <string>     // for string
#include <vector>     // for vector, vector<>::iter...

#include "ert.h"  // for ert_cmd_state
namespace Finn {
    struct DeviceWrapper;
}  // namespace Finn


namespace Finn {
    /**
     * @brief The Accelerator class wraps one or more Devices into a single Accelerator
     *
     */
    class Accelerator {
         private:
        /**
         * @brief A vector of DeviceHandler instances that belong to this accelerator
         *
         */
        std::vector<DeviceHandler> devices;

        /**
         * @brief A small prefix to determine where the log write came from
         *
         * @return std::string
         */
        static std::string loggerPrefix();

         public:
        Accelerator() = default;
        /**
         * @brief Construct a new Accelerator object using a list of DeviceWrappers
         *
         * @param deviceDefinitions Vector of @ref DeviceWrapper
         * @param synchronousInference Decides if synchronous or asynchronous inference should be used
         * @param hostBufferSize Size of ringbuffer to initialize
         */
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, bool synchronousInference, unsigned int hostBufferSize);
        /**
         * @brief Construct a new Accelerator object
         *
         */
        Accelerator(Accelerator&&) = default;
        /**
         * @brief Construct a new Accelerator object (deleted)
         *
         */
        Accelerator(const Accelerator&) = delete;
        /**
         * @brief Default move assignment operator
         *
         * @return Accelerator&
         */
        Accelerator& operator=(Accelerator&&) = default;
        /**
         * @brief Default copy assignment operator
         *
         * @return Accelerator&
         */
        Accelerator& operator=(const Accelerator&) = delete;
        /**
         * @brief Destroy the Accelerator object
         *
         */
        ~Accelerator() = default;


        /**
         * @brief Return a beginning iterator to the internal device vector (contains DeviceHandler)
         *
         * @return auto
         */
        std::vector<DeviceHandler>::iterator begin();

        /**
         * @brief Return an end iterator to the internal device vector (contains DeviceHandler)
         *
         * @return auto
         */
        std::vector<DeviceHandler>::iterator end();

        /**
         * @brief Return a reference to the deviceHandler with the given index. Crashes the driver if the index is invalid. To avoid accesses to uncertain indices, use Accelerator::containsDevice first.
         *
         * @param deviceIndex
         * @return DeviceHandler&
         */
        DeviceHandler& getDeviceHandler(unsigned int deviceIndex);

        /**
         * @brief Checks whether a device handler with the given device index exists
         *
         * @param deviceIndex
         * @return true
         * @return false
         */
        bool containsDevice(unsigned int deviceIndex);

        /**
         * @brief Factory to create a functon that can store data without index checks because they are checked beforehand. The created function only takes the data vector.
         * @attention (Currently on this commit) This also does NOT do checks on the length of the passed data vector and is _NOT THREAD SAFE_!
         *
         * @param deviceIndex
         * @param inputBufferKernelName
         * @return UncheckedStore
         */
        UncheckedStore storeFactory(unsigned int deviceIndex, const std::string& inputBufferKernelName);

        /**
         * @brief Set the Batch Size
         *
         * @param batchsize
         */
        void setBatchSize(uint batchsize);

        /**
         * @brief Run the given buffer. Returns false if no valid data was found to execute on.
         *
         * @param deviceIndex
         * @param inputBufferKernelName
         * @return true
         * @return false
         */
        bool run(unsigned int deviceIndex, const std::string& inputBufferKernelName);

        /**
         * @brief Return a vector of output samples.
         *
         * @param deviceIndex
         * @param outputBufferKernelName
         * @param forceArchival Whether or not to force a readout into archive. Necessary to get new data. Will be done automatically if a whole multiple of the buffer size is produced
         * @return std::vector<std::vector<uint8_t>>
         */
        Finn::vector<uint8_t> retrieveResults(unsigned int deviceIndex, const std::string& outputBufferKernelName, bool forceArchival);

        /**
         * @brief Execute the output kernel and return it's result. If a run fails, the function returns early, with the corresponding ert_cmd_state.
         *
         * @param deviceIndex
         * @param outputBufferKernelName
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples);

        /**
         * @brief Get the size of the buffer with the specified device index and buffer name
         *
         * @param ss
         * @param deviceIndex
         * @param bufferName
         * @return std::size_t
         */
        std::size_t size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName);
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
