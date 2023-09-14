#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include <filesystem>
#include <vector>


namespace Finn {

    // Fwd declarations
    class DeviceHandler;
    struct BufferDescriptor;

    /**
     * @brief Helper struct to structure input data for DeviceHandler creation
     *
     */
    struct DeviceWrapper {
        std::filesystem::path xclbin;
        std::string name;
        std::vector<BufferDescriptor> idmas;
        std::vector<BufferDescriptor> odmas;
    };


    /**
     * @brief The Accelerator class wraps one or more Devices into a single Accelerator
     *
     */
    class Accelerator {
         public:
        /**
         * @brief Construct a new Accelerator object using a list of DeviceWrappers
         *
         * @param deviceDefinitions Vector of @ref DeviceWrapper
         */
        explicit Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions);
        /**
         * @brief Construct a new Accelerator object using a single DeviceWrapper
         *
         * @param deviceWrapper
         */
        explicit Accelerator(const DeviceWrapper& deviceWrapper);
        /**
         * @brief Default move Constructor
         *
         */
        Accelerator(Accelerator&&) = default;
        /**
         * @brief Deleted copy constructor. Copying the Accelerator can result in multiple Accelerators managing the same Device
         *
         */
        Accelerator(const Accelerator&) = delete;
        /**
         * @brief Default move assignment operator
         *
         * @return Accelerator& other
         */
        Accelerator& operator=(Accelerator&&) = default;
        /**
         * @brief Deleted move assignment operator. Copying the Accelerator can result in multiple Accelerators managing the same Device
         *
         * @return Accelerator&
         */
        Accelerator& operator=(const Accelerator&) = delete;
        /**
         * @brief Destroy the Accelerator object. Default Implementation.
         *
         */
        ~Accelerator() = default;

         private:
        /**
         * @brief Vector of Devices managed by the Accelerator object.
         *
         */
        std::vector<DeviceHandler> devices;
    };


}  // namespace Finn

#endif  // ACCELERATOR_H
