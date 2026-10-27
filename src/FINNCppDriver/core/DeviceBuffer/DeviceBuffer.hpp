/**
 * @file DeviceBuffer.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements a wrapper to get data easier to and from the FPGAs
 * @version 2.0
 * @date 2023-12-20
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DEVICEBUFFER
#define DEVICEBUFFER

#include <FINNCppDriver/config/CompilationOptions.h>
#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/Logger.hpp>
#include <chrono>
#include <future>
#include <optional>
#include <span>
#include <thread>

#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

/**
 * @brief Magic value used by XRT to start kernel
 *
 */
constexpr uint32_t IP_START = 0x1;
/**
 * @brief ap_idle bit of the HLS control register
 *
 * @attention For the FINN IODMA kernels with numReps > 1, ap_idle is observed shortly before the last repetition has fully drained into the accelerator.
 * Starting the input kernel again right away corrupts the last sample of the previous batch (measured on U55C: 2-10% of 10-sample batches, even with a
 * separate buffer for the next batch). The asynchronous buffers therefore only start a batch once the previous one has completed on the output kernels
 * (see Finn::detail::BatchGate). ap_done (clear-on-read) is not an alternative: polling it lost completions and hung on the same hardware.
 */
constexpr uint32_t IP_IDLE = 0x4;
/**
 * @brief Magic value used by XRT as offset
 *
 */
constexpr uint32_t CSR_OFFSET = 0x0;

// Forward declares
enum ert_cmd_state;

namespace Finn {
    /**
     * @brief Parent class for DeviceBuffer objects.
     *
     * @tparam T Datatype in which the data is stored (e.g. uint8_t)
     */
    template<typename T>
    class DeviceBuffer {
         protected:
        /**
         * @brief Name of the buufer
         *
         */
        std::string name;
        /**
         * @brief Packed shape (Type T): (1,2,3)
         *
         */
        shapePacked_t shapePacked;
        /**
         * @brief Numbers of type T: When F has bitwidth 2, and T has bitwidth 8, the folded shape would be (1,2,10) and the packed (1,2,3) and thus 6
         *
         */
        size_t mapSize;
        /**
         * @brief XRT IP core associated with this Buffer
         *
         * Declared (and therefore constructed) before the buffer object: opening the IP establishes the process' context
         * on the loaded xclbin, which buffer allocation requires. Without it, re-creating buffers after the previous
         * ones were destroyed fails with EPERM.
         */
        xrt::ip assocIPCore;
        /**
         * @brief XRT buffer object; This is used to interact with FPGA memory
         *
         */
        xrt::bo internalBo;
        /**
         * @brief Mapped buffer; Part of the XRT buffer object
         *
         */
        T* map;
        /**
         * @brief 64 bit address of the buffer located on the FPGA card
         *
         */
        const long long bufAdr;

        /**
         * @brief Total size of data in elements
         *
         */
        std::size_t totalDataSize;
        /**
         * @brief Size of feature map
         *
         */
        std::size_t featureMapSize;

        /**
         * @brief Number of status-register polls done back-to-back before the wait starts sleeping between polls
         *
         * Each poll is a PCIe register read of a few microseconds, so this covers short kernel runs with minimal latency.
         */
        static constexpr std::size_t spinPolls = 2000;
        /**
         * @brief Pause between two status-register polls while spinning
         *
         * Zero by default: a single polling thread (synchronous inference) gets the lowest latency from back-to-back
         * reads. Buffers whose polling runs concurrently with kernel starts from another thread (the asynchronous
         * workers) set a short pause, because back-to-back register reads starve other register accesses in the XRT
         * driver (measured: a kernel start from another thread was delayed by up to 1 ms). Measured cost of a 2 us
         * pause on the synchronous path: 1.2 to 1.9 us per inference, which is why it is not applied there.
         */
        std::chrono::nanoseconds pollPause{0};

        /**
         * @brief Wait until the IP core reports idle
         *
         * Polls the control register; after @ref spinPolls polls it backs off to sleeping between polls so that a
         * thread waiting for a kernel that has nothing to do (e.g. the asynchronous output worker while no input is
         * pending) does not occupy a CPU core. See @ref IP_IDLE for the caveat on reusing input buffers.
         *
         * @param stopToken Token to request stopping the wait
         * @return true if the kernel is idle, false if the wait was stopped via the token
         */
        bool busyWait(std::stop_token stopToken = {}) {
            return waitFor([this] { return (assocIPCore.read_register(CSR_OFFSET) & IP_IDLE) == IP_IDLE; }, stopToken);
        }

        /**
         * @brief Wait until the ring DMA has completed descriptor count `posted`
         *
         * @param posted Descriptor count to wait for
         * @param stopToken Token to request stopping the wait
         * @return true if reached, false if the wait was stopped via the token
         */
        bool ringWait(uint32_t posted, std::stop_token stopToken = {}) {
            return waitFor([this, posted] { return ringReached(posted); }, stopToken);
        }

        /**
         * @brief Whether this buffer drives a free-running ring DMA
         *
         */
        bool isRingDma() const { return ring.has_value(); }

        /**
         * @brief Ring DMA: wait until every descriptor but the most recent one has completed
         *
         * Lets a double-buffered input fill one buffer while the kernel still reads the other.
         *
         * @param stopToken Token to request stopping the wait
         * @return true if reached, false if the wait was stopped via the token
         */
        bool ringWaitAllButLast(std::stop_token stopToken = {}) { return ringWait(ringPosted - 1, stopToken); }

        /**
         * @brief Poll until `done` returns true, spinning first and then backing off (see busyWait)
         *
         * @param done Condition, one register read per call
         * @param stopToken Token to request stopping the wait
         * @return true if the condition was met, false if the wait was stopped via the token
         */
        template<typename Done>
        bool waitFor(Done done, std::stop_token stopToken) {
            using namespace std::chrono_literals;
            std::size_t polls = 0;
            while (!stopToken.stop_requested()) {
                if (done()) {
                    return true;
                }
                if (++polls > spinPolls) {
                    std::this_thread::sleep_for(polls > 2 * spinPolls ? 200us : 20us);
                } else if (pollPause.count() > 0) {
                    // Leaves gaps between the reads for register accesses of other threads, see pollPause
                    const auto until = std::chrono::steady_clock::now() + pollPause;
                    while (std::chrono::steady_clock::now() < until) {
                    }
                }
            }
            return false;
        }

         private:
        /**
         * @brief Kernel register contents written by the last execute, so unchanged registers are not rewritten (one PCIe write each)
         *
         */
        uint32_t lastRepetitions = 0;
        uint64_t lastAddress = ~uint64_t{0};

        /**
         * @brief Register map of a free-running ring DMA; empty for register DMAs
         *
         */
        std::optional<RingControl> ring;
        /**
         * @brief Descriptors posted to the ring DMA so far (the value last written to its prod register)
         *
         */
        uint32_t ringPosted = 0;

        /**
         * @brief Number of descriptors the ring DMA has completed
         *
         */
        uint32_t ringCompleted() { return assocIPCore.read_register(ring->cons); }

         protected:
        /**
         * @brief Whether the ring DMA has completed descriptor count `posted` (wrap-around safe)
         *
         */
        bool ringReached(uint32_t posted) { return static_cast<int32_t>(ringCompleted() - posted) >= 0; }

        /**
         * @brief Descriptors posted to the ring DMA so far
         *
         */
        uint32_t ringDescriptorsPosted() const { return ringPosted; }

        /**
         * @brief Get flags for buffer object creation based on host memory access
         *
         * @param hostMemoryAccess Whether host memory access is enabled
         * @return consteval static xrt::bo::flags Buffer object flags
         */
        consteval static xrt::bo::flags getFlags(bool hostMemoryAccess) {
            if (hostMemoryAccess) {
                return xrt::bo::flags::host_only;
            }
            return xrt::bo::flags::normal;
        }

         public:
        /**
         * @brief Construct a new Device Buffer object
         *
         * @param pCUName Name of compute unit
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param pMemGroup memory group (bank) the compute unit's memory port reaches; the buffer is allocated there
         */
        DeviceBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1, const std::optional<RingControl>& pRing = std::nullopt, unsigned int pMemGroup = 0)
            : name(pCUName),
              shapePacked(pShapePacked),
              mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked) * batchSize)),
              assocIPCore(xrt::ip(device, pDevUUID, pCUName)),  // Using xrt::kernel after this point leads to a total bricking of the FPGA card!!
              internalBo(xrt::bo(device, mapSize * sizeof(T), DeviceBuffer::getFlags(Finn::Options::hostMemoryAccess), pMemGroup)),
              map(internalBo.template map<T*>()),
              bufAdr(internalBo.address()),
              ring(pRing) {
            shapePacked[0] = batchSize;
            if (ring) {
                // Continue from the kernel's count: it never restarts, so its counters survive the previous process
                ringPosted = ringCompleted();
                const uint32_t prod = assocIPCore.read_register(ring->prod);
                if (prod != ringPosted) {
                    FINN_LOG(loglevel::warning) << "Ring DMA " << name << " still has " << (prod - ringPosted) << " descriptors of a previous process pending; reset the card if its outputs were not collected";
                    ringPosted = prod;
                }
                FINN_LOG(loglevel::info) << "Ring DMA " << name << ": " << ring->slots << " descriptor slots, " << ringPosted << " descriptors completed so far";
            }
            FINN_LOG(loglevel::info) << "New Device Buffer of size " << mapSize * sizeof(T) << "bytes with group id " << pMemGroup << "\n";
            FINN_LOG(loglevel::info) << "Host Memory Access enabled: " << Finn::Options::hostMemoryAccess << "\n";
            FINN_LOG(loglevel::info) << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
            std::fill(map, map + mapSize, 0);
            totalDataSize = FinnUtils::shapeToElements(pShapePacked) * batchSize;
            featureMapSize = totalDataSize / shapePacked[0];
            FINN_LOG(loglevel::info) << "Map has totalSize " << totalDataSize << " and featureMapSize " << featureMapSize << "\n";
        }

        /**
         * @brief Deleted move constructor. Buffers are only ever handled through std::shared_ptr; moving one would also
         * have to carry totalDataSize, featureMapSize and the cached kernel registers.
         */
        DeviceBuffer(DeviceBuffer&& buf) noexcept = delete;

        /**
         * @brief Construct a new Device Buffer object (Deleted copy constructor)
         *
         * @param buf
         */
        DeviceBuffer(const DeviceBuffer& buf) noexcept = delete;

        /**
         * @brief Prepare the DeviceBuffer for shutdown
         *
         * This function is called before the application is shutting down.
         * It can be used to release resources or perform cleanup tasks.
         * The default implementation does nothing.
         */
        virtual void prepareForShutdown() {
            // Base implementation does nothing
            FINN_LOG(loglevel::info) << "Preparing " << name << " for shutdown";
        }

        /**
         * @brief Destroy the Device Buffer object
         *
         */
        virtual ~DeviceBuffer() { FINN_LOG(loglevel::info) << "Destructing DeviceBuffer " << name << std::endl; };

        /**
         * @brief Deleted move assignment operator
         *
         * @param buf
         * @return DeviceBuffer&
         */
        DeviceBuffer& operator=(DeviceBuffer&& buf) = delete;

        /**
         * @brief Deleted copy assignment operator
         *
         * @param buf
         * @return DeviceBuffer&
         */
        DeviceBuffer& operator=(const DeviceBuffer& buf) = delete;

        /**
         * @brief Get the size in bytes of the buffer
         *
         * @return size_t Size in bytes
         */
        virtual size_t getSizeInBytes() { return totalDataSize * sizeof(T); }

#ifdef UNITTEST
        /**
         * @brief Device address of the internal buffer (unit tests)
         *
         */
        long long testGetAddress() const { return bufAdr; }

        /**
         * @brief Memory group the internal buffer was allocated in (unit tests)
         *
         */
        unsigned int testGetMemoryGroup() const { return internalBo.get_memory_group(); }
#endif

        /**
         * @brief Get the feature map size
         *
         * @return size_t Feature map size
         */
        virtual size_t getFeatureMapSize() { return featureMapSize; }

        /**
         * @brief Get the batch size
         *
         * @return size_t Batch size
         */
        virtual size_t getBatchSize() { return this->shapePacked[0]; }

        /**
         * @brief Get the total data size
         *
         * @return size_t Total data size
         */
        virtual size_t getTotalDataSize() { return totalDataSize; }

        /**
         * @brief Get the name of the device buffer
         *
         * @return std::string&
         */
        virtual std::string& getName() { return name; }

        /**
         * @brief Get the Packed Shape object
         *
         * @return shape_t&
         */
        virtual shape_t& getPackedShape() { return shapePacked; }

        /**
         * @brief Run the associated kernel
         *
         * @return true Success
         * @return false Fail
         */
        virtual bool run() = 0;

        /**
         * @brief Wait for the kernel to complete execution
         *
         * @param stopToken Token to request stopping the wait
         * @return true The kernel finished
         * @return false The wait was stopped via the token before the kernel finished
         */
        virtual bool wait(std::stop_token stopToken = {}) { return ring ? ringWait(ringPosted, stopToken) : busyWait(stopToken); };

         protected:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        virtual std::string loggerPrefix() { return "[" + std::string(Finn::type_name<decltype(*this)>()) + " - " + name + "] "; }

        /**
         * @brief Synchronizes the first elements of the buffer with the FPGA memory
         *
         * @param elements Number of elements of type T to transfer (converted to bytes internally)
         */
        virtual void sync(std::size_t elements) = 0;

        /**
         * @brief Execute the kernel on this buffer with specified repetitions
         *
         * @param repetitions Number of repetitions to execute (default: 1)
         */
        void execute(const uint32_t repetitions = 1) { executeAt(static_cast<uint64_t>(bufAdr), repetitions); }

        /**
         * @brief Execute the kernel on the device buffer at the given address with specified repetitions
         *
         * Only registers whose value changed since the last execute are written.
         *
         * @param address Device address of the buffer the kernel should use
         * @param repetitions Number of repetitions to execute
         */
        void executeAt(const uint64_t address, const uint32_t repetitions) {
            if (ring) {
                postDescriptor(address, repetitions);
                return;
            }
            constexpr uint32_t offset_buf = 0x10;
            constexpr uint32_t offset_rep = 0x1C;

            if (address != lastAddress) {
                assocIPCore.write_register(offset_buf, static_cast<uint32_t>(address));
                assocIPCore.write_register(offset_buf + 4, static_cast<uint32_t>(address >> 32));
                lastAddress = address;
            }
            if (repetitions != lastRepetitions) {
                assocIPCore.write_register(offset_rep, repetitions);
                lastRepetitions = repetitions;
            }

            // Start inference
            assocIPCore.write_register(CSR_OFFSET, IP_START);
        }

        /**
         * @brief Post one descriptor to the ring DMA: move `frames` samples to or from `address`
         *
         * Waits for a free slot first (at most `slots` descriptors are outstanding). The kernel starts on it as soon as
         * prod is written; completion is observed through ringWait()/wait().
         *
         * @param address Device address of the buffer
         * @param frames Number of samples
         */
        void postDescriptor(const uint64_t address, const uint32_t frames) {
            ringWait(ringPosted - ring->slots + 1);
            const uint32_t slot = 4 * (ringPosted % ring->slots);
            assocIPCore.write_register(ring->addrLo + slot, static_cast<uint32_t>(address));
            assocIPCore.write_register(ring->addrHi + slot, static_cast<uint32_t>(address >> 32));
            assocIPCore.write_register(ring->frames + slot, frames);
            ++ringPosted;
            assocIPCore.write_register(ring->prod, ringPosted);
        }
    };

    /**
     * @brief @ref SyncDeviceInputBuffer
     *
     * @tparam T
     */
    template<typename T>
    class SyncDeviceInputBuffer;
    /**
     * @brief @ref AsyncDeviceInputBuffer
     *
     * @tparam T
     */
    template<typename T>
    class AsyncDeviceInputBuffer;

    /**
     * @brief Abstract base class that defines interfaces that need to be fulfilled by the DeviceInputBuffers
     *
     * @tparam T
     */
    template<typename T>
    class DeviceInputBuffer : public DeviceBuffer<T> {
         protected:
        /**
         * @brief Specifies if DeviceBuffer is input or output buffer
         *
         */
        const IO ioMode = IO::INPUT;

         public:
        /**
         * @brief Construct a new Device Input Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         */
        DeviceInputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1, const std::optional<RingControl>& pRing = std::nullopt,
                          unsigned int pMemGroup = 0)
            : DeviceBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize, pRing, pMemGroup){};

        /**
         * @brief Store the given vector of data in the FPGA mem map
         * @attention This function is NOT THREAD SAFE!
         *
         * @param data
         * @return true
         * @return false
         */
        virtual bool store(std::span<const T> data) = 0;

         protected:
        /**
         * @brief Sync data from the map to the device.
         *
         */
        void sync(std::size_t elements) override { this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE, elements * sizeof(T), 0); }

         private:
        template<typename InputIt>
        static bool storeImpl(InputIt first, InputIt last) {
            Finn::logAndError<std::runtime_error>("Base Implementation called! This should not happen.");
            return false;
        }

#ifdef UNITTEST
         public:
        Finn::vector<T> testGetMap() {
            Finn::vector<T> temp;
            for (size_t i = 0; i < FinnUtils::shapeToElements(this->shapePacked); i++) {
                temp.push_back(this->map[i]);
            }
            return temp;
        }
        void testSyncBackFromDevice() { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE); }
        xrt::bo& testGetInternalBO() { return this->internalBo; }
#endif
    };


    /**
     * @brief Abstract base class that defines interfaces that need to be fulfilled by the DeviceOutputBuffers
     *
     * @tparam T
     */
    template<typename T>
    class DeviceOutputBuffer : public DeviceBuffer<T> {
         protected:
        /**
         * @brief Specifies IO mode of buffer
         *
         */
        const IO ioMode = IO::OUTPUT;
         public:
        /**
         * @brief Construct a new Device Output Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         */
        DeviceOutputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1, const std::optional<RingControl>& pRing = std::nullopt,
                           unsigned int pMemGroup = 0)
            : DeviceBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize, pRing, pMemGroup){};

        /**
         * @brief Get the data from the buffer and return it as a vector
         *
         * @return Finn::vector<T>
         */
        virtual Finn::vector<T> getData(const std::size_t& numItems) = 0;

        /**
         * @brief Sync data from the FPGA back to the host
         *
         */
        virtual bool read() = 0;

        /**
         * @brief Register a callback that is called when data is available in the queue (Only for AsyncDeviceOutputBuffer)
         *
         * @param callback Callback function that takes the number of items available in the queue
         */
        virtual void registerCallback(std::function<void(std::size_t)> callback) {
            // Default implementation does nothing
            // This can be overridden by derived classes if needed
            Finn::logAndError<std::runtime_error>("Callback not supported by this DeviceOutputBuffer implementation.");
        }

        virtual void drain() {
            // Default implementation does nothing
            // This can be overridden by derived classes if needed
            Finn::logAndError<std::runtime_error>("Drain not supported by this DeviceOutputBuffer implementation.");
        }

         protected:
        /**
         * @brief Sync data from the FPGA into the memory map
         *
         */
        void sync(std::size_t elements) override { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, elements * sizeof(T), 0); }

#ifdef UNITTEST
         public:
        std::vector<T> testGetMap() {
            std::vector<T> temp;
            for (size_t i = 0; i < FinnUtils::shapeToElements(this->shapePacked); ++i) {
                temp.push_back(this->map[i]);
            }
            return temp;
        }

        template<typename IteratorType>
        void testSetMap(IteratorType first, IteratorType last) {
            if (std::distance(first, last) > this->mapSize) {
                Finn::logAndError<std::length_error>("Error setting test map. Sizes dont match");
            }
            for (unsigned int i = 0; i < std::distance(first, last); ++i) {
                this->map[i] = first[i];
            }
        }

        void testSetMap(const std::vector<T>& data) { testSetMap(data.begin(), data.end()); }

        void testSetMap(const Finn::vector<T>& data) { testSetMap(data.begin(), data.end()); }

        xrt::bo& testGetInternalBO() { return this->internalBo; }
#endif
    };
}  // namespace Finn

#endif  // DEVICEBUFFER
