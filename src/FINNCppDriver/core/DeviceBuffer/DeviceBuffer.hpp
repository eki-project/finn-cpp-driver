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

#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/RingBuffer.hpp>
#include <boost/type_index.hpp>
#include <future>
#include <span>
#include <thread>
#include <chrono>

#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

constexpr uint32_t IP_START = 0x1;
constexpr uint32_t IP_IDLE = 0x4;
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
         * @brief XRT buffer object; This is used to interact with FPGA memory
         *
         */
        xrt::bo internalBo;
        /**
         * @brief XRT IP core associated with this Buffer
         *
         */
        xrt::ip assocIPCore;
        /**
         * @brief Mapped buffer; Part of the XRT buffer object
         *
         */
        T* map;
        /**
         * @brief 64 bit adress of the buffer located on the FPGA card
         *
         */
        const long long bufAdr;
        /**
         * @brief Logger
         *
         */
        logger_type& logger;

        void busyWait() {
            // Wait until the IP is DONE
            uint32_t axi_ctrl = 0;
            while ((axi_ctrl & IP_IDLE) != IP_IDLE) {
                axi_ctrl = assocIPCore.read_register(CSR_OFFSET);
            }
        }

         private:
        unsigned int getGroupId(const xrt::device& device, const xrt::uuid& uuid, const std::string& computeUnit) { return xrt::kernel(device, uuid, computeUnit).group_id(0); }

        /**
         * @brief Used for deciding if execute needs to write data registers or not
         *
         */
        uint32_t oldRepetitions = 0;

    public:
        /**
         * @brief Construct a new Device Buffer object
         *
         * @param pCUName Name of compute unit
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         */
        DeviceBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1)
            : name(pCUName),
            shapePacked(pShapePacked),
            mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked)* batchSize)),
            internalBo(xrt::bo(device, mapSize * sizeof(T), getGroupId(device, pDevUUID, pCUName))),
            map(internalBo.template map<T*>()),
            assocIPCore(xrt::ip(device, pDevUUID, pCUName)),//Using xrt::kernel/getGroupId after this point leads to a total bricking of the FPGA card!!
            bufAdr(internalBo.address()),
            logger(Logger::getLogger()) {
            shapePacked[0] = batchSize;
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                << "New Device Buffer of size " << mapSize * sizeof(T) << "bytes with group id " << 0 << "\n";
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
            std::fill(map, map + mapSize, 0);
        }

        /**
         * @brief Construct a new Device Buffer object (Move constructor)
         *
         * @param buf
         */
        DeviceBuffer(DeviceBuffer&& buf) noexcept
            : name(std::move(buf.name)),
            shapePacked(std::move(buf.shapePacked)),
            mapSize(buf.mapSize),
            internalBo(std::move(buf.internalBo)),
            assocIPCore(std::move(buf.assocIPCore)),
            map(std::move(buf.map)),
            bufAdr(internalBo.address()),
            logger(Logger::getLogger()) {}

        /**
         * @brief Construct a new Device Buffer object (Deleted copy constructor)
         *
         * @param buf
         */
        DeviceBuffer(const DeviceBuffer& buf) noexcept = delete;

        /**
         * @brief Destroy the Device Buffer object
         *
         */
        virtual ~DeviceBuffer() { FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] Destructing DeviceBuffer " << name << "\n"; };

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
         * @brief Returns a specific size parameter of DeviceBuffer. Size parameter selected with @see SIZE_SPECIFIER
         *
         * @param ss @see SIZE_SPECIFIER
         * @return size_t
         */
        virtual size_t size(SIZE_SPECIFIER ss) = 0;

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

        virtual bool wait() {
            busyWait();
            return true;
        };

         protected:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        virtual std::string loggerPrefix() { return "[" + finnBoost::typeindex::type_id<decltype(this)>().pretty_name() + " - " + name + "] "; }

        /**
         * @brief Synchronizes the Buffer data to the data on the FPGA
         *
         * @param bytes
         */
        virtual void sync(std::size_t bytes) = 0;

        void execute(const uint32_t repetitions = 1) {
            // writes the buffer adress
            constexpr uint32_t offset_buf = 0x10;
            constexpr uint32_t offset_rep = 0x1C;

            // If repetition number is the same as for the last call, then nothing has to be written before starting the Kernel
            if (repetitions == oldRepetitions) {
                assocIPCore.write_register(CSR_OFFSET, IP_START);
                return;
            }
            oldRepetitions = repetitions;

            assocIPCore.write_register(offset_buf, bufAdr);
            assocIPCore.write_register(offset_buf + 4, bufAdr >> 32);

            // writes the repetitions
            assocIPCore.write_register(offset_rep, repetitions);

            // Start inference
            assocIPCore.write_register(CSR_OFFSET, IP_START);
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
        DeviceInputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1) : DeviceBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize) {};

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
        void sync(std::size_t bytes) override { this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE, bytes, 0); }

    private:
        template<typename InputIt>
        static bool storeImpl(InputIt first, InputIt last) {
            FinnUtils::logAndError<std::runtime_error>("Base Implementation called! This should not happen.");
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
        /**
         * @brief Data storage until data is requested by user
         *
         */
        Finn::vector<T> longTermStorage;
        /**
         * @brief Timeout for kernels
         *
         */
        unsigned int msExecuteTimeout = 1000;

    public:
        /**
         * @brief Construct a new Device Output Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         */
        DeviceOutputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize = 1) : DeviceBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize) {};

        /**
         * @brief Return stored data from storage
         *
         * @return Finn::vector<T>
         */
        virtual Finn::vector<T> getData() = 0;
        /**
         * @brief Sync data from the FPGA back to the host
         *
         */
        virtual bool read() = 0;

    protected:
        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync(std::size_t bytes) override { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, bytes, 0); }

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
                FinnUtils::logAndError<std::length_error>("Error setting test map. Sizes dont match");
            }
            for (unsigned int i = 0; i < std::distance(first, last); ++i) {
                this->map[i] = first[i];
            }
        }

        void testSetMap(const std::vector<T>& data) { testSetMap(data.begin(), data.end()); }

        void testSetMap(const Finn::vector<T>& data) { testSetMap(data.begin(), data.end()); }

        unsigned int testGetLongTermStorageSize() const { return longTermStorage.size(); }
        xrt::bo& testGetInternalBO() { return this->interalBo; }
        Finn::vector<T>& testGetLTS() { return longTermStorage; }
#endif
    };
}  // namespace Finn

#endif  // DEVICEBUFFER
