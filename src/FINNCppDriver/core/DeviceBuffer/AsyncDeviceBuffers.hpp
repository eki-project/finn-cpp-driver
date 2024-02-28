/**
 * @file AsyncDeviceBuffers.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements asynchronous FPGA interfaces to transfer data to and from the FPGA.
 * @version 1.0
 * @date 2023-12-20
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef ASYNCDEVICEBUFFERS
#define ASYNCDEVICEBUFFERS

#include <FINNCppDriver/utils/FinnUtils.h>

#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <functional>
#include <thread>

#include "ert.h"

namespace Finn {

    namespace detail {
        /**
         * @brief Wrapper that contains the ringbuffer used by Asynchronous Input & Output Buffers
         *
         * @tparam T Type of the data stored in the ringbuffer
         */
        template<typename T>
        class AsyncBufferWrapper {
             protected:
            /**
             * @brief Internal Ringbuffer used by all asynchronous buffers
             *
             */
            RingBuffer<T, true> ringBuffer;

            /**
             * @brief Construct a new Async Buffer Wrapper object
             *
             * @param ringBufferSizeFactor Number of batch elements that should be able to be stored
             * @param elementsPerPart Number of values per batch element
             */
            AsyncBufferWrapper(unsigned int ringBufferSizeFactor, std::size_t elementsPerPart) : ringBuffer(RingBuffer<T, true>(ringBufferSizeFactor, elementsPerPart)) {
                if (ringBufferSizeFactor == 0) {
                    FinnUtils::logAndError<std::runtime_error>("DeviceBuffer of size 0 cannot be constructed!");
                }
                FINN_LOG(Logger::getLogger(), loglevel::info) << "[AsyncDeviceBuffer] Max buffer size:" << ringBufferSizeFactor << "*" << elementsPerPart << "\n";
            }

            /**
             * @brief Destroy the Async Buffer Wrapper object
             *
             */
            ~AsyncBufferWrapper() = default;
            /**
             * @brief Construct a new Async Buffer Wrapper object (Move construction)
             *
             * @param buf
             */
            AsyncBufferWrapper(AsyncBufferWrapper&& buf) noexcept : ringBuffer(std::move(buf.ringBuffer)) {}
            /**
             * @brief Construct a new Async Buffer Wrapper object (Deleted Copy constructor)
             *
             * @param buf
             */
            AsyncBufferWrapper(const AsyncBufferWrapper& buf) noexcept = delete;
            /**
             * @brief Deleted move assignment operator
             *
             * @param buf
             * @return AsyncBufferWrapper&
             */
            AsyncBufferWrapper& operator=(AsyncBufferWrapper&& buf) = delete;
            /**
             * @brief Deleted copy assignment operator
             *
             * @param buf
             * @return AsyncBufferWrapper&
             */
            AsyncBufferWrapper& operator=(const AsyncBufferWrapper& buf) = delete;
#ifdef UNITTEST
             public:
            RingBuffer<T, true>& testGetRingBuffer() { return this->ringBuffer; }
#endif
        };
    }  // namespace detail

    /**
     * @brief Implements the asynchronous input buffer that transfers input to the FPGA device
     *
     * @tparam T Datatype of the data transfered. Most likely always uint8_t
     */
    template<typename T>
    class AsyncDeviceInputBuffer : public DeviceInputBuffer<T>, public detail::AsyncBufferWrapper<T> {
         private:
        friend class DeviceInputBuffer<T>;
        std::jthread workerThread;

        /**
         * @brief Internal run method used by the runner thread
         *
         */
        void runInternal(std::stop_token stoken) {
            const std::size_t elementCount = this->ringBuffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE);
            while (!stoken.stop_requested()) {
                if (!this->loadMap(stoken)) {  // blocks
                    break;
                }
                this->sync(elementCount);
                this->execute();
            }
            FINN_LOG(this->logger, loglevel::info) << "Asynchronous Input buffer runner terminated";
        }

         public:
        /**
         * @brief Construct a new Async Device Input Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param ringBufferSizeFactor size of ringbuffer in input elements (batch elements)
         */
        AsyncDeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceInputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked),
              detail::AsyncBufferWrapper<T>(ringBufferSizeFactor, FinnUtils::shapeToElements(pShapePacked)),
              workerThread(std::jthread(std::bind_front(&AsyncDeviceInputBuffer::runInternal, this))){};

        /**
         * @brief Construct a new Async Device Input Buffer object
         *
         * @param buf
         */
        AsyncDeviceInputBuffer(AsyncDeviceInputBuffer&& buf) noexcept = default;
        /**
         * @brief Construct a new Async Device Input Buffer object (Deleted)
         *
         * @param buf
         */
        AsyncDeviceInputBuffer(const AsyncDeviceInputBuffer& buf) noexcept = delete;
        /**
         * @brief Destroy the Async Device Input Buffer object
         *
         */
        ~AsyncDeviceInputBuffer() override {
            FINN_LOG(this->logger, loglevel::info) << "Destructing Asynchronous input buffer";
            workerThread.request_stop();  // Joining will be handled automatically by destruction
        };
        /**
         * @brief Deleted move assignment
         *
         * @param buf
         * @return AsyncDeviceInputBuffer&
         */
        AsyncDeviceInputBuffer& operator=(AsyncDeviceInputBuffer&& buf) = delete;
        /**
         * @brief Deleted copy assignment
         *
         * @param buf
         * @return AsyncDeviceInputBuffer&
         */
        AsyncDeviceInputBuffer& operator=(const AsyncDeviceInputBuffer& buf) = delete;

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) override { return this->ringBuffer.size(ss); }

        /**
         * @brief Store the given data in the ring buffer
         *
         * @param data
         * @return true Store was successful
         * @return false Store failed
         */
        bool store(std::span<const T> data) override { return this->ringBuffer.store(data.begin(), data.end()); }

         protected:
        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        ert_cmd_state execute(uint batchsize = 1) override {
            auto runCall = this->associatedKernel(this->internalBo, batchsize);
            return runCall.wait();
        }

        /**
         * @brief  Load data from the ring buffer into the memory map of the device.
         * @attention Invalidates the data that was moved to map
         *
         * @return true
         * @return false
         */
        bool loadMap(std::stop_token stoken) {
            FINN_LOG(this->logger, loglevel::info) << "Data transfer of input data to FPGA!\n";
            return this->ringBuffer.read(this->map, stoken);
        }

        /**
         * @brief Not supported for AsyncInputBuffer
         *
         * @return true
         * @return false
         */
        bool run() override { FinnUtils::logAndError<std::runtime_error>("Calling run is not supported for Async execution! This is done automatically."); }
    };


    /**
     * @brief Implements the asynchronous output buffer that transfers output from the FPGA device
     *
     * @tparam T Datatype of the data transfered. Most likely always uint8_t
     */
    template<typename T>
    class AsyncDeviceOutputBuffer : public DeviceOutputBuffer<T>, public detail::AsyncBufferWrapper<T> {
        std::mutex ltsMutex;
        std::jthread workerThread;

         private:
        void readInternal(std::stop_token stoken) {
            FINN_LOG_DEBUG(this->logger, loglevel::info) << this->loggerPrefix() << "Starting to read from the device";
            const std::size_t elementCount = this->ringBuffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE);
            while (!stoken.stop_requested()) {
                auto outExecuteResult = execute();
                std::cout << outExecuteResult << "\n";
                if (outExecuteResult != ERT_CMD_STATE_COMPLETED && outExecuteResult != ERT_CMD_STATE_ERROR && outExecuteResult != ERT_CMD_STATE_ABORT) {
                    continue;
                }
                if (outExecuteResult == ERT_CMD_STATE_ERROR || outExecuteResult == ERT_CMD_STATE_ABORT) {
                    FINN_LOG(this->logger, loglevel::error) << "A problem has occured during the read process of the FPGA output.";
                    continue;
                }
                this->sync(elementCount);
                saveMap();
                if (this->ringBuffer.full()) {  // TODO(linusjun): Allow registering of callback for this event?
                    archiveValidBufferParts();
                }
            }
        }

         public:
        /**
         * @brief Construct a new Async Device Output Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param ringBufferSizeFactor size of ringbuffer in input elements (batch elements)
         */
        AsyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked),
              detail::AsyncBufferWrapper<T>(ringBufferSizeFactor, FinnUtils::shapeToElements(pShapePacked)),
              workerThread(std::jthread(std::bind_front(&AsyncDeviceOutputBuffer::readInternal, this))){};

        /**
         * @brief Construct a new Async Device Output Buffer object (Move constructor)
         *
         * @param buf
         */
        AsyncDeviceOutputBuffer(AsyncDeviceOutputBuffer&& buf) noexcept = default;
        /**
         * @brief Construct a new Async Device Output Buffer object (Deleted copy constructor)
         *
         * @param buf
         */
        AsyncDeviceOutputBuffer(const AsyncDeviceOutputBuffer& buf) noexcept = delete;
        /**
         * @brief Destroy the Async Device Output Buffer object
         *
         */
        ~AsyncDeviceOutputBuffer() override {
            FINN_LOG(this->logger, loglevel::info) << "Destruction Asynchronous output buffer";
            workerThread.request_stop();  // Joining will be handled automatically by destruction
        };

        /**
         * @brief Deleted move assignment operator
         *
         * @param buf
         * @return AsyncDeviceOutputBuffer&
         */
        AsyncDeviceOutputBuffer& operator=(AsyncDeviceOutputBuffer&& buf) = delete;

        /**
         * @brief Deleted copy assignment operator
         *
         * @param buf
         * @return AsyncDeviceOutputBuffer&
         */
        AsyncDeviceOutputBuffer& operator=(const AsyncDeviceOutputBuffer& buf) = delete;

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) override { return this->ringBuffer.size(ss); }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        void archiveValidBufferParts() {
            std::lock_guard guard(ltsMutex);
            this->longTermStorage.reserve(this->longTermStorage.size() + this->ringBuffer.size());
            this->ringBuffer.readAllValidParts(std::back_inserter(this->longTermStorage));
        }

        /**
         * @brief Return the archive.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> getData() {
            std::lock_guard guard(ltsMutex);
            Finn::vector<T> tmp(this->longTermStorage);
            clearArchive();
            return tmp;
        }

        /**
         * @brief Reserve enough storage for the expectedEntries number of entries. Note however that because this is a vec of vecs, this only allocates memory for the pointers, not the data itself.
         *
         * @param expectedEntries
         */
        void allocateLongTermStorage([[maybe_unused]] unsigned int expectedEntries) { this->longTermStorage.reserve(expectedEntries * this->ringBuffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE)); }

         protected:
        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() {
            FINN_LOG(this->logger, loglevel::info) << "Data transfer of output from FPGA!\n";
            this->ringBuffer.template store<T*>(this->map, this->ringBuffer.size(SIZE_SPECIFIER::FEATUREMAP_SIZE));
        }

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         */
        ert_cmd_state execute(uint batchsize = 1) override {
            auto run = this->associatedKernel(this->internalBo, batchsize);
            return run.wait(500);
        }

        /**
         * @brief Clear the archive of all it's entries
         *
         */
        void clearArchive() { this->longTermStorage.clear(); }

        /**
         * @brief Not supported by AsyncOutputBuffer
         *
         * @return unsigned int
         */
        unsigned int getMsExecuteTimeout() const override { return 0; }

        /**
         * @brief Not supported by AsyncOutputBuffer
         *
         * @param val
         */
        void setMsExecuteTimeout(unsigned int val) override {}

        /**
         * @brief Not supported by the AsyncDeviceOutputBuffer.
         *
         * @param samples
         * @return ert_cmd_state Returns error every time.
         */
        ert_cmd_state read(unsigned int samples) override { return ert_cmd_state::ERT_CMD_STATE_ERROR; }
    };
}  // namespace Finn


#endif  // ASYNCDEVICEBUFFERS
