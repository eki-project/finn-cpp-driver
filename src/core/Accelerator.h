#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <cinttypes>   // for uint8_t
#include <filesystem>  // for path
#include <string>      // for string
#include <vector>      // for vector

#include "../utils/ConfigurationStructs.h"
#include "DeviceHandler.h"  // for BufferDescriptor, DeviceHandler
#include "ert.h"


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

        /**
         * @brief Return a beginning iterator to the internal device vector (contains DeviceHandler) 
         * 
         * @return auto 
         */
        auto begin();

        /**
         * @brief Return an end iterator to the internal device vector (contains DeviceHandler) 
         * 
         * @return auto 
         */
        auto end();

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
         * @brief Factory to create a functon that can store data without index checks because they are checked beforehand. The created function only takes the data vector
         *
         * @param deviceIndex
         * @param inputBufferKernelName
         * @return UncheckedStore
         */
        UncheckedStore storeFactory(unsigned int deviceIndex, const std::string& inputBufferKernelName);


        /**
         * @brief Store data in the device handler with the given deviceIndex, and in the buffer with the given inputBufferKernelName.
         *
         * @param data
         * @param deviceIndex If such a deviceIndex does not exist, use the first (0) device handler. If it doesnt exist, crash.
         * @param inputBufferKernelName
         * @return true The write was successfull
         * @return false The buffer is full and data needs to be run first
         */
        bool store(const std::vector<uint8_t>& data, unsigned int deviceIndex, const std::string& inputBufferKernelName);

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
         * @param samples The number of samples to read
         * @param forceArchive Whether or not to force a readout into archive. Necessary to get new data. Will be done automatically if a whole multiple of the buffer size is produced
         * @return std::vector<std::vector<uint8_t>>
         */
        std::vector<std::vector<uint8_t>> retrieveResults(unsigned int deviceIndex, const std::string& outputBufferKernelName, bool forceArchival);

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
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName);
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
