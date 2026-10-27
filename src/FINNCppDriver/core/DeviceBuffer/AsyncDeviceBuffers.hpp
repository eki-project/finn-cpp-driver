/**
 * @file AsyncDeviceBuffers.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements asynchronous FPGA interfaces to transfer data to and from the FPGA.
 * @version 1.1
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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "ert.h"

namespace Finn {

    namespace detail {
        /**
         * @brief Orders the kernel starts of the asynchronous workers of one device
         *
         * Per batch N the order is: every output kernel completed batch N-1 and the host read it, then the input
         * workers announce batch N, then every output kernel starts its run for batch N, then the input kernels start
         * batch N. All three rules were measured on U55C with the jet network:
         * - Starting input batch N+1 before output batch N completed corrupted the last sample of batch N (the input
         *   kernels report idle shortly before their last repetition has drained). FINN's reference driver has the same
         *   order.
         * - Starting an output run after its input run, or racing the two starts, corrupts the last sample of the batch
         *   with 3 percent (started after) down to 0.1 percent (racing) probability: the output kernel must already be
         *   waiting on the stream when the first data arrives. The synchronous path always starts the output kernel
         *   first and never showed the corruption.
         * - An output run started ahead of any announced input batch is left pending on the card when the process
         *   ends and swallows the first sample of the next process (every later output is shifted by one sample until
         *   the card is reset). Output runs are therefore only started for announced batches, and shutdown drains them.
         * Host work for the next batch (packing, queueing, DMA into the spare buffer) still overlaps with the FPGA.
         */
        class BatchGate {
            std::mutex mutex;
            std::condition_variable_any cv;
            std::vector<uint64_t> completedPerOutput;  ///< batches completed per output buffer
            std::vector<uint64_t> startedPerOutput;    ///< output kernel runs started per output buffer
            std::vector<uint64_t> announcedPerInput;   ///< batches announced (about to start) per input buffer
            std::size_t registeredOutputs = 0;
            std::size_t registeredInputs = 0;
            bool active = true;

            template<typename Counters>
            static bool allAtLeast(const Counters& counters, uint64_t value) {
                for (const auto counter : counters) {
                    if (counter < value) {
                        return false;
                    }
                }
                return true;
            }

            template<typename Pred>
            bool waitUntil(const std::stop_token& stoken, Pred satisfied) {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, stoken, [&] { return !active || satisfied(); });
                return satisfied();
            }

             public:
            /**
             * @brief Construct a gate for a fixed number of input and output buffers
             *
             * The counts are fixed up front because the buffers are constructed one after another and each one starts
             * its worker immediately: a wait over an empty set of counters would be vacuously satisfied, and an output
             * worker could run ahead of the inputs before they exist.
             */
            BatchGate(std::size_t inputs, std::size_t outputs) : completedPerOutput(outputs, 0), startedPerOutput(outputs, 0), announcedPerInput(inputs, 0) {}

            /**
             * @brief Registers an output buffer
             *
             * @return Index to pass to @ref outputStarted and @ref completed
             */
            std::size_t registerOutput() {
                const std::lock_guard<std::mutex> lock(mutex);
                if (registeredOutputs >= completedPerOutput.size()) {
                    Finn::logAndError<std::runtime_error>("More output buffers registered at the batch gate than it was constructed for");
                }
                return registeredOutputs++;
            }

            /**
             * @brief Registers an input buffer
             *
             * @return Index to pass to @ref inputAnnounced
             */
            std::size_t registerInput() {
                const std::lock_guard<std::mutex> lock(mutex);
                if (registeredInputs >= announcedPerInput.size()) {
                    Finn::logAndError<std::runtime_error>("More input buffers registered at the batch gate than it was constructed for");
                }
                return registeredInputs++;
            }

            /**
             * @brief Records that output buffer `index` finished one more batch
             */
            void completed(std::size_t index) {
                {
                    const std::lock_guard<std::mutex> lock(mutex);
                    ++completedPerOutput[index];
                }
                cv.notify_all();
            }

            /**
             * @brief Input buffer `index` commits to starting its next batch (it will start it even if stopped meanwhile)
             */
            void inputAnnounced(std::size_t index) {
                {
                    const std::lock_guard<std::mutex> lock(mutex);
                    ++announcedPerInput[index];
                }
                cv.notify_all();
            }

            /**
             * @brief Records that output buffer `index` started its kernel run for one more batch
             */
            void outputStarted(std::size_t index) {
                {
                    const std::lock_guard<std::mutex> lock(mutex);
                    ++startedPerOutput[index];
                }
                cv.notify_all();
            }

            /**
             * @brief Blocks until every input buffer has announced at least `batches` batches
             *
             * Returns true as soon as that holds, even after a stop request or shutdown, so that an output worker still
             * serves a batch the inputs committed to. Returns false only if the condition does not hold when the gate is
             * shut down or the token is stopped.
             */
            bool waitForInputsAnnounced(uint64_t batches, const std::stop_token& stoken) {
                return waitUntil(stoken, [&] { return allAtLeast(announcedPerInput, batches); });
            }

            /**
             * @brief Whether every input buffer has announced at least `batches` batches (does not block)
             */
            bool inputsAnnounced(uint64_t batches) {
                const std::lock_guard<std::mutex> lock(mutex);
                return allAtLeast(announcedPerInput, batches);
            }

            /**
             * @brief Blocks until every output buffer has started its run for at least `batches` batches
             *
             * Not interruptible by a stop token: the caller has announced the batch and the output workers will start
             * it. Returns false only if the gate was shut down first, which the shutdown order of DeviceHandler prevents.
             */
            bool waitForOutputsStarted(uint64_t batches) { return waitUntil(std::stop_token{}, [&] { return allAtLeast(startedPerOutput, batches); }); }

            /**
             * @brief Blocks until every output buffer has completed at least `batches` batches
             *
             * @return false if shut down or stopped before that happened
             */
            bool waitForCompleted(uint64_t batches, const std::stop_token& stoken) {
                std::unique_lock<std::mutex> lock(mutex);
                const auto ready = [&] { return !active || allAtLeast(completedPerOutput, batches); };
                cv.wait(lock, stoken, ready);
                return active && !stoken.stop_requested() && ready();
            }

            /**
             * @brief Wakes all waiters and makes further waits return immediately
             */
            void shutdown() {
                {
                    const std::lock_guard<std::mutex> lock(mutex);
                    active = false;
                }
                cv.notify_all();
            }
        };

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
            explicit AsyncBufferWrapper(std::size_t expectedMaxQueueSize) : queue(expectedMaxQueueSize * featureMapCount) { FINN_LOG(loglevel::info) << "[AsyncDeviceBuffer] Max buffer size:" << queue.capacity() << "\n"; }

            ~AsyncBufferWrapper() = default;

            // The queue is used by a worker thread that captures `this`, so these objects must never move
            AsyncBufferWrapper(AsyncBufferWrapper&&) = delete;
            AsyncBufferWrapper(const AsyncBufferWrapper&) = delete;
            AsyncBufferWrapper& operator=(AsyncBufferWrapper&&) = delete;
            AsyncBufferWrapper& operator=(const AsyncBufferWrapper&) = delete;

            /**
             * @brief Joins the worker thread of a buffer after requesting it to stop, logging if that takes unusually long
             *
             * @param worker Thread to join
             * @param name Buffer name for the log
             */
            static void stopWorker(std::jthread& worker, const std::string& name) {
                if (!worker.joinable()) {
                    return;
                }
                worker.request_stop();
                const auto start = std::chrono::steady_clock::now();
                worker.join();
                const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
                if (took > std::chrono::seconds(1)) {
                    FINN_LOG(loglevel::warning) << "Worker thread for " << name << " needed " << took.count() << " ms to stop";
                } else {
                    FINN_LOG(loglevel::info) << "Worker thread for " << name << " exited cleanly";
                }
            }
#ifdef UNITTEST
             public:
            DynamicSPSCQueue<T>& testGetQueue() { return this->queue; }
#endif

             public:
            /**
             * @brief Return the number of elements currently held in the queue
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
        /**
         * @brief Second device buffer: batches alternate between the two buffers so the worker never writes into the
         * buffer the input kernel was last started on. The kernel can report idle slightly before its last repetition
         * has finished reading (see @ref IP_IDLE); with two buffers that read is never disturbed.
         */
        xrt::bo spareBo;
        T* spareMap;
        uint64_t spareAdr;
        std::shared_ptr<detail::BatchGate> gate;  ///< shared with the output buffers of the same device
        std::size_t gateIndex = 0;                ///< this buffer's index in the gate
        std::jthread workerThread;

        /**
         * @brief Internal run method used by the runner thread
         *
         * Moves one batch at a time from the queue into the inactive device buffer and runs the input kernel on it. A
         * stop request (or a queue shutdown) can wake the dequeue with a partial batch; the kernel must then not be
         * started, because it would stream stale buffer contents into the accelerator and leave data in flight.
         */
        void runInternal(std::stop_token stoken) {
            bool useSpare = false;
            uint64_t started = 0;
            while (!stoken.stop_requested()) {
                T* target = useSpare ? spareMap : this->map;
                xrt::bo& bo = useSpare ? spareBo : this->internalBo;
                FINN_LOG_DEBUG(loglevel::info) << "Data transfer of input data to FPGA!\n";
                const std::size_t loaded = this->queue.dequeue_bulk(target, this->totalDataSize, stoken);
                if (loaded != this->totalDataSize) {
                    break;
                }
                bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, loaded * sizeof(T), 0);
                if (this->isRingDma()) {
                    // Ring DMAs never restart and take data whenever it comes, so batches only follow each other: post
                    // this one, then wait until the kernel has read the previous one, whose buffer is filled next
                    gate->inputAnnounced(gateIndex);
                    ++started;
                    this->executeAt(useSpare ? spareAdr : static_cast<uint64_t>(this->bufAdr), this->shapePacked[0]);
                    if (!this->ringWaitAllButLast(stoken)) {
                        FINN_LOG_DEBUG(loglevel::info) << "Stopped waiting for the input kernel";
                        break;
                    }
                    useSpare = !useSpare;
                    continue;
                }
                // Batch k may only start once the outputs of batch k-1 are complete, and only after the output kernels
                // started their run for batch k, see BatchGate
                if (!gate->waitForCompleted(started, stoken)) {
                    break;
                }
                gate->inputAnnounced(gateIndex);  // commits to this batch: it is started even if a stop arrives meanwhile
                if (!gate->waitForOutputsStarted(started + 1)) {
                    break;
                }
                ++started;
                this->executeAt(useSpare ? spareAdr : static_cast<uint64_t>(this->bufAdr), this->shapePacked[0]);
                if (!this->wait(stoken)) {
                    FINN_LOG_DEBUG(loglevel::info) << "Stopped waiting for the input kernel";
                    break;
                }
                useSpare = !useSpare;
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
        AsyncDeviceInputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize, std::shared_ptr<detail::BatchGate> pGate,
                               const std::optional<RingControl>& pRing = std::nullopt, unsigned int pMemGroup = 0)
            : DeviceInputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize, pRing, pMemGroup),
              detail::AsyncBufferWrapper<T>(batchSize * FinnUtils::shapeToElements(pShapePacked)),
              spareBo(device, this->mapSize * sizeof(T), DeviceBuffer<T>::getFlags(Finn::Options::hostMemoryAccess), pMemGroup),
              spareMap(spareBo.template map<T*>()),
              spareAdr(spareBo.address()),
              gate(std::move(pGate)),
              gateIndex(gate->registerInput()) {
            std::fill(spareMap, spareMap + this->mapSize, 0);
            this->pollPause = std::chrono::microseconds{2};  // the output worker starts kernels while this buffer polls
            // Started last, once every member the worker touches is constructed
            workerThread = std::jthread(std::bind_front(&AsyncDeviceInputBuffer::runInternal, this));
        }

        AsyncDeviceInputBuffer(AsyncDeviceInputBuffer&& buf) noexcept = delete;
        AsyncDeviceInputBuffer(const AsyncDeviceInputBuffer& buf) noexcept = delete;
        AsyncDeviceInputBuffer& operator=(AsyncDeviceInputBuffer&& buf) = delete;
        AsyncDeviceInputBuffer& operator=(const AsyncDeviceInputBuffer& buf) = delete;

        /**
         * @brief Destroy the Async Device Input Buffer object
         *
         */
        ~AsyncDeviceInputBuffer() override {
            prepareForShutdown();
            FINN_LOG(loglevel::info) << "Destructing Asynchronous input buffer" << std::endl;
        };

        /**
         * @brief Stop the worker thread and wait for it. Safe to call more than once.
         */
        void prepareForShutdown() override {
            if (!workerThread.joinable()) {
                return;
            }
            FINN_LOG(loglevel::info) << "Stopping Asynchronous input buffer" << std::endl;
            DeviceInputBuffer<T>::prepareForShutdown();
            this->queue.shutdown();  // wakes a worker that is blocked waiting for input; a gate wait wakes on the stop token
            this->stopWorker(workerThread, this->name);  // the gate stays active: the output workers still drain started batches
        }

        /**
         * @brief Store the given data in the ring buffer. Blocks while the ring buffer is full.
         *
         * @param data
         * @return true Store was successful
         * @return false Store failed (the buffer is shutting down)
         */
        bool store(std::span<const T> data) override {
            if (this->queue.enqueue_bulk(data.data(), data.size()) == data.size()) {
                FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "Stored " << data.size() << " elements in the ring buffer";
                return true;
            }
            FINN_LOG_DEBUG(loglevel::error) << this->loggerPrefix() << "Failed to store data in the ring buffer.";
            return false;
        }

         protected:
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
        std::shared_ptr<detail::BatchGate> gate;         ///< shared with the input buffers of the same device
        std::size_t gateIndex = 0;                       ///< this buffer's index in the gate
        std::optional<xrt::bo> spareBo;                  ///< ring DMAs only: second buffer, written by the kernel while the host collects the other one
        T* spareMap = nullptr;
        uint64_t spareAdr = 0;
        std::jthread workerThread;
        std::mutex callbackMutex;                        ///< Guards callback against registerCallback() racing with the worker
        std::function<void(std::size_t)> callback = {};  ///< Callback that is called when data is available in the queue

         private:
        /**
         * @brief Internal read method used by the runner thread
         *
         * Keeps the output kernel running so that results are collected as soon as the accelerator produces them. On a
         * stop request the map does not hold a complete batch, so nothing is forwarded to the queue.
         */
        void readInternal(std::stop_token stoken) {
            FINN_LOG_DEBUG(loglevel::info) << "Starting to read from the device";
            if (this->isRingDma()) {
                readRing(stoken);
                FINN_LOG(loglevel::info) << "Asynchronous Output buffer runner terminated";
                return;
            }
            for (uint64_t batch = 1;; ++batch) {
                // Only run the kernel for a batch the inputs committed to; this also serves announced batches after a
                // stop request so that no run is left pending on the card, see BatchGate
                if (!gate->waitForInputsAnnounced(batch, stoken)) {
                    break;
                }
                this->execute(this->shapePacked[0]);
                gate->outputStarted(gateIndex);  // the input kernels may start now
                this->wait();  // no stop token: the batch is in flight and the run completes once it has passed through
                this->sync(this->totalDataSize);
                gate->completed(gateIndex);  // the batch is out and read by the host: the inputs may start the next one (same order as FINN's reference driver)
                if (!saveMap(stoken)) {
                    continue;  // stopping with a full queue: the data is dropped, the device state is clean
                }
                const std::size_t queued = this->queue.size();
                notify(queued - (queued % this->totalDataSize));
            }
            FINN_LOG(loglevel::info) << "Asynchronous Output buffer runner terminated";
        }

        /**
         * @brief readInternal for a ring DMA: double-buffered
         *
         * The descriptor of the next batch is posted while the host still syncs and copies the current one, so the kernel
         * always has a buffer to write into. Single-buffered, it would hold back the whole pipeline during every sync and
         * copy. As for register DMAs, a descriptor is only posted for a batch the inputs committed to, and a posted batch
         * is always waited for, so no descriptor is left pending on the card.
         */
        void readRing(const std::stop_token& stoken) {
            const uint32_t base = this->ringDescriptorsPosted();  // batch b completes with descriptor count base + b
            uint64_t posted = 0;
            const auto post = [&] {
                ++posted;
                this->executeAt(posted % 2 == 1 ? static_cast<uint64_t>(this->bufAdr) : spareAdr, this->shapePacked[0]);
                gate->outputStarted(gateIndex);
            };
            for (uint64_t batch = 1;; ++batch) {
                if (posted < batch) {
                    if (!gate->waitForInputsAnnounced(batch, stoken)) {
                        break;
                    }
                    post();
                }
                // No stop token: the batch is in flight and completes once it has passed through
                this->waitFor(
                    [&] {
                        if (posted == batch && gate->inputsAnnounced(batch + 1)) {
                            post();
                        }
                        return this->ringReached(base + static_cast<uint32_t>(batch));
                    },
                    {});
                xrt::bo& bo = batch % 2 == 1 ? this->internalBo : *spareBo;
                bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, this->totalDataSize * sizeof(T), 0);
                gate->completed(gateIndex);
                if (!saveMap(batch % 2 == 1 ? this->map : spareMap, stoken)) {
                    continue;  // stopping with a full queue: the data is dropped, the device state is clean
                }
                const std::size_t queued = this->queue.size();
                notify(queued - (queued % this->totalDataSize));
            }
        }

        /**
         * @brief Invokes the registered callback, if any
         *
         * @param numItems Number of complete batches worth of elements available in the queue
         */
        void notify(std::size_t numItems) {
            std::function<void(std::size_t)> cb;
            {
                const std::lock_guard<std::mutex> lock(callbackMutex);
                cb = callback;
            }
            if (cb) {
                cb(numItems);
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
        AsyncDeviceOutputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize, std::shared_ptr<detail::BatchGate> pGate,
                                const std::optional<RingControl>& pRing = std::nullopt, unsigned int pMemGroup = 0)
            : DeviceOutputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize, pRing, pMemGroup),
              detail::AsyncBufferWrapper<T>(2 * batchSize * FinnUtils::shapeToElements(pShapePacked)),  // Make output buffer map twice as large to circumvent a very rare deadlock in the case where one thread handles IO alone.
              gate(std::move(pGate)),
              gateIndex(gate->registerOutput()) {
            if (pRing) {
                spareBo.emplace(device, this->mapSize * sizeof(T), DeviceBuffer<T>::getFlags(Finn::Options::hostMemoryAccess), pMemGroup);
                spareMap = spareBo->template map<T*>();
                spareAdr = spareBo->address();
                std::fill(spareMap, spareMap + this->mapSize, 0);
            }
            this->pollPause = std::chrono::microseconds{2};  // the input worker starts kernels while this buffer polls
            // Started last, once every member the worker touches (callback mutex, callback, gate) is constructed
            workerThread = std::jthread(std::bind_front(&AsyncDeviceOutputBuffer::readInternal, this));
        };

        AsyncDeviceOutputBuffer(AsyncDeviceOutputBuffer&& buf) noexcept = delete;
        AsyncDeviceOutputBuffer(const AsyncDeviceOutputBuffer& buf) noexcept = delete;
        AsyncDeviceOutputBuffer& operator=(AsyncDeviceOutputBuffer&& buf) = delete;
        AsyncDeviceOutputBuffer& operator=(const AsyncDeviceOutputBuffer& buf) = delete;

        /**
         * @brief Destroy the Async Device Output Buffer object
         *
         */
        ~AsyncDeviceOutputBuffer() override {
            prepareForShutdown();
            FINN_LOG(loglevel::info) << "Destruction Asynchronous output buffer" << std::endl;
        };

        /**
         * @brief Stop the worker thread and wait for it. Safe to call more than once.
         */
        void prepareForShutdown() override {
            if (!workerThread.joinable()) {
                return;
            }
            DeviceOutputBuffer<T>::prepareForShutdown();
            this->queue.shutdown();  // wakes a worker that is blocked on a full queue
            gate->shutdown();        // wakes a worker waiting for inputs that will not come; must run after the input buffers were stopped
            this->stopWorker(workerThread, this->name);
        }

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
         *  @brief Return the next numItems elements, blocking until they are available.
         *
         * @return Finn::vector<T> The requested elements; shorter only if the buffer was shut down while waiting
         */
        Finn::vector<T> getData(const std::size_t& numItems) override {
            Finn::vector<T> tmp(numItems);
            const std::size_t received = this->queue.dequeue_bulk(tmp.begin(), numItems);
            if (received != numItems) {
                FINN_LOG(loglevel::warning) << this->loggerPrefix() << "Only " << received << " of " << numItems << " requested elements were available before shutdown";
                tmp.resize(received);
            }
            return tmp;
        }

        void registerCallback(std::function<void(std::size_t)> newCallback) override {
            const std::lock_guard<std::mutex> lock(callbackMutex);
            this->callback = std::move(newCallback);
        }

        void drain() override {
            // Discard all queued items
            this->queue.clear();
            FINN_LOG_DEBUG(loglevel::info) << "Drained the AsyncDeviceOutputBuffer queue.";
        }

         protected:
        /**
         * @brief Store the contents of the memory map into the ring buffer. Blocks while the ring buffer is full.
         *
         * @return false if the buffer was shut down before the data could be stored
         */
        bool saveMap(const std::stop_token& stoken) { return saveMap(this->map, stoken); }

        /**
         * @brief Store the contents of the given buffer map into the ring buffer. Blocks while the ring buffer is full.
         *
         * @return false if the buffer was shut down before the data could be stored
         */
        bool saveMap(const T* source, const std::stop_token& stoken) {
            FINN_LOG_DEBUG(loglevel::info) << "Data transfer of output from FPGA!\n";
            if (this->queue.enqueue_bulk(source, this->totalDataSize, stoken) == this->totalDataSize) {
                FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "Stored " << this->totalDataSize << " elements in the FIFO";
                return true;
            }
            FINN_LOG_DEBUG(loglevel::error) << this->loggerPrefix() << "Failed to store data in the FIFO.";
            return false;
        }
    };
}  // namespace Finn


#endif  // ASYNCDEVICEBUFFERS
