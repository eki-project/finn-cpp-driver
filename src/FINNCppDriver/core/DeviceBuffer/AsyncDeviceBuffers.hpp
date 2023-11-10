/**
 * @file AsyncDeviceBuffers.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements asynchronous FPGA interfaces to transfer data to and from the FPGA.
 * @version 0.1
 * @date 2023-11-10
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef ASYNCDEVICEBUFFERS
#define ASYNCDEVICEBUFFERS

#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>

#include "ert.h"

namespace Finn {
    template<typename T>
    class SyncDeviceInputBuffer : public DeviceInputBuffer<T> {
        std::mutex runMutex;

         public:
        SyncDeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceInputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};
        /**
         * @brief Move Constructor
         * @attention This move constructor is NOT THREAD SAFE
         *
         * @param buf
         */
        //! NOT THREAD SAFE
        // cppcheck-suppress missingMemberCopy
        SyncDeviceInputBuffer(SyncDeviceInputBuffer&& buf) noexcept : DeviceInputBuffer<T>(buf.name, buf.shapePacked, buf.mapSize, buf.internalBo, buf.associatedKernel, buf.map, buf.ringBuffer){};

        SyncDeviceInputBuffer(const SyncDeviceInputBuffer& buf) noexcept = delete;
        ~SyncDeviceInputBuffer() override = default;
        SyncDeviceInputBuffer& operator=(SyncDeviceInputBuffer&& buf) = delete;
        SyncDeviceInputBuffer& operator=(const SyncDeviceInputBuffer& buf) = delete;

         private:
        friend class DeviceInputBuffer<T>;

        /**
         * @brief Store the given data in the ring buffer
         * @attention This function is NOT THREAD SAFE!
         *
         * @tparam InputIt The type of the iterator
         * @param first
         * @param last
         * @return true
         * @return false
         */
        template<typename InputIt>
        bool storeImpl(InputIt first, InputIt last) {
            static_assert(std::is_same<typename std::iterator_traits<InputIt>::value_type, T>::value);
            return false;
        }

         public:
        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        ert_cmd_state execute() override { return ert_cmd_state::ERT_CMD_STATE_ERROR; }

        /**
         * @brief Execute the first valid data that is found in the buffer. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() override { return false; }

        /**
         * @brief Load data from the ring buffer into the memory map of the device.
         *
         * @attention Invalidates the data that was moved to map
         *
         * @return true
         * @return false
         */
        bool loadMap() override { return false; }
    };

    template<typename T>
    class SyncDeviceOutputBuffer : public DeviceOutputBuffer<T> {
         public:
        SyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};

         public:
        /**
         * @brief Reserve enough storage for the expectedEntries number of entries. Note however that because this is a vec of vecs, this only allocates memory for the pointers, not the data itself.
         *
         * @param expectedEntries
         */
        void allocateLongTermStorage([[maybe_unused]] unsigned int expectedEntries) override {}

        /**
         * @brief Get the the kernel timeout in miliseconds
         *
         * @return unsigned int
         */
        unsigned int getMsExecuteTimeout() const override { return 0; }

        /**
         * @brief Set the kernel timeout in miliseconds
         *
         * @param val
         */
        void setMsExecuteTimeout([[maybe_unused]] unsigned int val) override {}

        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync() override {}

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        ert_cmd_state execute() override { return ert_cmd_state::ERT_CMD_STATE_ERROR; }

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() override {}

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        void archiveValidBufferParts() override {}

        /**
         * @brief Return the archive.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> retrieveArchive() override { return {}; }

        /**
         * @brief Clear the archive of all it's entries
         *
         */
        void clearArchive() override {}

        /**
         * @brief Read the specified number of samples. If a read fails, immediately return. If all are successfull, the kernel state of the last run is returned
         *
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int samples) override { return ert_cmd_state::ERT_CMD_STATE_ERROR; }
    };
}  // namespace Finn


#endif  // ASYNCDEVICEBUFFERS
