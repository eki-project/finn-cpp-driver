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
#include <FINNCppDriver/utils/SPSCQueue.hpp>
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
            constexpr static size_t featureMapCount = 5;  //< Number of feature maps that can be buffered in the queue
            /**
             * @brief Internal queue used by all asynchronous buffers
             *
             */
            DynamicSPSCQueue<T> queue;

            /**
             * @brief Construct a new Async Buffer Wrapper object
             *
             * @param expectedMaxQueueSize Expected maximum size of the queue
             */
            AsyncBufferWrapper(std::size_t expectedMaxQueueSize) : queue(expectedMaxQueueSize * featureMapCount) { FINN_LOG(loglevel::info) << "[AsyncDeviceBuffer] Max buffer size:" << queue.size() << "\n"; }

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
            AsyncBufferWrapper(AsyncBufferWrapper&& buf) noexcept {}
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
            DynamicSPSCQueue<T>& testGetQueue() { return this->queue; }
#endif

             public:
            /**
             * @brief Return the size of the buffer as specified by the argument.
             *
             * @return size_t
             */
            virtual size_t size() { return this->queue.size(); }
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
            while (!stoken.stop_requested()) {
                this->sync(this->loadMap(stoken));
                this->execute(this->shapePacked[0]);
                bool success = this->wait(stoken);  // Wait until the kernel is done executing
                if (!success) {
                    FINN_LOG_DEBUG(loglevel::error) << "Kernel execution failed";
                }
            }
            FINN_LOG(loglevel::info) << "Asynchronous Input buffer runner terminated";
        }

         public:
        /**
         * @brief Construct a new Async Device Input Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param batchSize size of ringbuffer in input elements (batch elements)
         */
        AsyncDeviceInputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize)
            : DeviceInputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize),
              detail::AsyncBufferWrapper<T>(batchSize * FinnUtils::shapeToElements(pShapePacked)),
              workerThread(std::jthread(std::bind_front(&AsyncDeviceInputBuffer::runInternal, this))) {}

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
        ~AsyncDeviceInputBuffer() override { FINN_LOG(loglevel::info) << "Destructing Asynchronous input buffer" << std::endl; };

        /**
         * @brief Prepare the buffer for shutdown
         *
         * This method will signal the worker thread to stop and wait for it to finish.
         */
        void prepareForShutdown() override {
            FINN_LOG(loglevel::info) << "Stopping Asynchronous input buffer" << std::endl;
            DeviceInputBuffer<T>::prepareForShutdown();

            // Signal worker thread to stop
            this->queue.shutdown();
            workerThread.request_stop();

            // Attempt to join with timeout
            auto joinFuture = std::async(std::launch::async, [this]() {
                if (workerThread.joinable()) {
                    workerThread.join();
                }
            });

            if (joinFuture.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
                FINN_LOG(loglevel::warning) << "Worker thread for " << this->name << " did not exit cleanly" << std::endl;
                // Thread will be detached automatically when jthread is destroyed
                throw std::runtime_error("Worker thread did not exit cleanly within timeout period");
            } else {
                FINN_LOG(loglevel::info) << "Worker thread for " << this->name << " exited cleanly" << std::endl;
            }
        }

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
         * @brief Store the given data in the ring buffer
         *
         * @param data
         * @return true Store was successful
         * @return false Store failed
         */
        bool store(std::span<const T> data) override {
            if (this->queue.enqueue_bulk(data.data(), data.size()) == data.size()) {
                FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "Stored " << data.size() << " elements in the ring buffer";
                return true;
            } else {
                FINN_LOG_DEBUG(loglevel::error) << this->loggerPrefix() << "Failed to store data in the ring buffer.";
                return false;
            }
        }

         protected:
        /**
         * @brief  Load data from the ring buffer into the memory map of the device.
         * @attention Invalidates the data that was moved to map
         *
         * @return Number of bytes loaded into the map
         */
        size_t loadMap(std::stop_token stoken) {
            FINN_LOG_DEBUG(loglevel::info) << "Data transfer of input data to FPGA!\n";
            FINN_LOG_DEBUG(loglevel::info) << "Queue size: " << this->queue.size() << "\n";
            return this->queue.dequeue_bulk(this->map, this->totalDataSize, stoken);
        }

        /**
         * @brief Not supported for AsyncInputBuffer
         *
         * @return true
         * @return false
         */
        bool run() override { return false; }
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
        std::function<void(std::size_t)> callback = [](std::size_t numItems) {};  ///< Callback that is called when data is available in the queue

         private:
        void readInternal(std::stop_token stoken) {
            FINN_LOG_DEBUG(loglevel::info) << "Starting to read from the device";
            while (!stoken.stop_requested()) {
                this->execute(this->shapePacked[0]);
                bool success = this->wait(stoken);  // Wait until the kernel is done executing
                if (!success) {
                    FINN_LOG_DEBUG(loglevel::error) << "Kernel execution failed";
                }
                this->sync(this->totalDataSize);
                saveMap();
                callback(this->queue.size() - (this->queue.size() % this->totalDataSize));  // Notify that data is available in the queue
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
         * @param batchSize batch size of the output
         */
        AsyncDeviceOutputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize)
            : DeviceOutputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize),
              detail::AsyncBufferWrapper<T>(2 * batchSize * FinnUtils::shapeToElements(pShapePacked)),  // Make output buffer map twice as large to circumvent a very rare deadlock in the case where one thread handles IO alone.
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
        ~AsyncDeviceOutputBuffer() override { FINN_LOG(loglevel::info) << "Destruction Asynchronous output buffer" << std::endl; };

        /**
         * @brief Prepare the buffer for shutdown
         *
         * This method will signal the worker thread to stop and wait for it to finish.
         */
        void prepareForShutdown() override {
            DeviceOutputBuffer<T>::prepareForShutdown();

            // Signal worker thread to stop
            this->queue.shutdown();
            workerThread.request_stop();

            // Attempt to join with timeout
            auto joinFuture = std::async(std::launch::async, [this]() {
                if (workerThread.joinable()) {
                    workerThread.join();
                }
            });

            if (joinFuture.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
                FINN_LOG(loglevel::warning) << "Worker thread for " << this->name << " did not exit cleanly" << std::endl;
                // Thread will be detached automatically when jthread is destroyed
                throw std::runtime_error("Worker thread did not exit cleanly within timeout period");
            } else {
                FINN_LOG(loglevel::info) << "Worker thread for " << this->name << " exited cleanly" << std::endl;
            }
        }

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
         * @brief Not supported by the AsyncDeviceOutputBuffer.
         *
         * @return false
         */
        bool read() override { return false; }

        /**
         * @brief Not supported for AsyncDeviceOutputBuffer
         *
         * @return true
         * @return false
         */
        bool run() override { return false; }

        /**
         *  @brief Return the data contained in the FPGA Buffer map.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> getData(const std::size_t& numItems) override {
            Finn::vector<T> tmp(numItems);
            this->queue.dequeue_bulk(tmp.begin(), numItems);
            return tmp;
        }

        void registerCallback(std::function<void(std::size_t)> callback) override {
            // Register a callback that is called when data is available in the queue
            this->callback = callback;
        }

        void drain() override {
            // Drain the queue by reading all available items
            T item;
            while (this->queue.try_dequeue(item)) {}
            FINN_LOG_DEBUG(loglevel::info) << "Drained the AsyncDeviceOutputBuffer queue.";
        }

         protected:
        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        bool saveMap() {
            FINN_LOG_DEBUG(loglevel::info) << "Data transfer of output from FPGA!\n";
            if (this->queue.enqueue_bulk(this->map, this->totalDataSize) == this->totalDataSize) {
                FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "Stored " << this->totalDataSize << " elements in the FIFO";
                return true;
            } else {
                FINN_LOG_DEBUG(loglevel::error) << this->loggerPrefix() << "Failed to store data in the FIFO.";
                return false;
            }
        }
    };
}  // namespace Finn


#endif  // ASYNCDEVICEBUFFERS
