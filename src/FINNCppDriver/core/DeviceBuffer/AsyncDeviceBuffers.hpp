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

#include <FINNCppDriver/utils/FinnUtils.h>

#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <thread>

#include "ert.h"

namespace Finn {

    namespace detail {
        template<typename T>
        class AsyncBufferWrapper {
             protected:
            RingBuffer<T, true> ringBuffer;

            AsyncBufferWrapper(unsigned int ringBufferSizeFactor, std::size_t elementsPerPart) : ringBuffer(RingBuffer<T, true>(ringBufferSizeFactor, elementsPerPart)) {
                if (ringBufferSizeFactor == 0) {
                    FinnUtils::logAndError<std::runtime_error>("DeviceBuffer of size 0 cannot be constructed!");
                }
                FINN_LOG(Logger::getLogger(), loglevel::info) << "[SyncDeviceBuffer] Max buffer size:" << ringBufferSizeFactor << "*" << elementsPerPart << "\n";
            }

            ~AsyncBufferWrapper() = default;
            AsyncBufferWrapper(AsyncBufferWrapper&& buf) noexcept : ringBuffer(std::move(buf.ringBuffer)) {}
            AsyncBufferWrapper(const AsyncBufferWrapper& buf) noexcept = delete;
            AsyncBufferWrapper& operator=(AsyncBufferWrapper&& buf) = delete;
            AsyncBufferWrapper& operator=(const AsyncBufferWrapper& buf) = delete;
#ifdef UNITTEST
             public:
            RingBuffer<T, true>& testGetRingBuffer() { return this->ringBuffer; }
#endif
        };
    }  // namespace detail

    template<typename T>
    class AsyncDeviceInputBuffer : public DeviceInputBuffer<T>, public detail::AsyncBufferWrapper<T> {
         private:
        friend class DeviceInputBuffer<T>;
        std::jthread workerThread;

        /**
         * @brief Store the given data in the ring buffer
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
            return this->ringBuffer.store(first, last);
        }

        /**
         * @brief Internal run method used by the runner thread
         *
         */
        void runInternal(std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
                this->loadMap();  // blocks
                this->sync();
                this->execute();
            }
        }

         public:
        AsyncDeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceInputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked), detail::AsyncBufferWrapper<T>(ringBufferSizeFactor, FinnUtils::shapeToElements(pShapePacked)), workerThread(runInternal){};

        AsyncDeviceInputBuffer(AsyncDeviceInputBuffer&& buf) noexcept = default;
        AsyncDeviceInputBuffer(const AsyncDeviceInputBuffer& buf) noexcept = delete;
        ~AsyncDeviceInputBuffer() override {
            workerThread.request_stop();  // Joining will be handled automatically by destruction
        };
        AsyncDeviceInputBuffer& operator=(AsyncDeviceInputBuffer&& buf) = delete;
        AsyncDeviceInputBuffer& operator=(const AsyncDeviceInputBuffer& buf) = delete;

        /**
         * @brief Execute the first valid data that is found in the buffer. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() override { FinnUtils::logAndError<std::runtime_error>("Calling run is not supported for Async execution! This is done automatically."); }

         protected:
        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        ert_cmd_state execute() override {
            auto runCall = this->associatedKernel(this->internalBo, 1);
            runCall.wait();
            return runCall.state();
        }

        /**
         * @brief Load data from the ring buffer into the memory map of the device.
         *
         * @attention Invalidates the data that was moved to map
         *
         * @return true
         * @return false
         */
        bool loadMap() { return this->ringBuffer.read(this->map); }
    };

    template<typename T>
    class AsyncDeviceOutputBuffer : public DeviceOutputBuffer<T> {
         public:
        AsyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};

        AsyncDeviceOutputBuffer(AsyncDeviceOutputBuffer&& buf) noexcept = default;
        AsyncDeviceOutputBuffer(const AsyncDeviceOutputBuffer& buf) noexcept = delete;
        ~AsyncDeviceOutputBuffer() override = default;
        AsyncDeviceOutputBuffer& operator=(AsyncDeviceOutputBuffer&& buf) = delete;
        AsyncDeviceOutputBuffer& operator=(const AsyncDeviceOutputBuffer& buf) = delete;

        /**
         * @brief Read the specified number of samples. If a read fails, immediately return. If all are successfull, the kernel state of the last run is returned
         *
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int samples) override { return ert_cmd_state::ERT_CMD_STATE_ERROR; }

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

         protected:
        /**
         * @brief Reserve enough storage for the expectedEntries number of entries. Note however that because this is a vec of vecs, this only allocates memory for the pointers, not the data itself.
         *
         * @param expectedEntries
         */
        void allocateLongTermStorage([[maybe_unused]] unsigned int expectedEntries) override {}


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
         * @brief Clear the archive of all it's entries
         *
         */
        void clearArchive() override {}
    };
}  // namespace Finn


#endif  // ASYNCDEVICEBUFFERS
