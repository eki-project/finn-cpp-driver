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
#include <span>

#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

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
         * @brief XRT kernel associated with this Buffer
         *
         */
        xrt::kernel associatedKernel;
        /**
         * @brief Mapped buffer; Part of the XRT buffer object
         *
         */
        T* map;
        /**
         * @brief Logger
         *
         */
        logger_type& logger;


         public:
        /**
         * @brief Construct a new Device Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         */
        DeviceBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked)
            : name(pName),
              shapePacked(pShapePacked),
              mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
              internalBo(xrt::bo(device, mapSize * sizeof(T), pAssociatedKernel.group_id(0))),
              associatedKernel(pAssociatedKernel),
              map(internalBo.template map<T*>()),
              logger(Logger::getLogger()) {
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                                             << "New Device Buffer of size " << mapSize * sizeof(T) << "bytes with group id " << pAssociatedKernel.group_id(0) << "\n";
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                                             << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
            for (std::size_t i = 0; i < mapSize; ++i) {
                map[i] = 0;
            }
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
              associatedKernel(std::move(buf.associatedKernel)),
              map(std::move(buf.map)),
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

         protected:
        /**
         * @brief Internal constructor used by the move constructors of the sub classes !!!NOT THREAD SAFE!!!
         *
         * @param pName
         * @param pShapePacked
         * @param pMapSize
         * @param pInternalBo
         * @param pAssociatedKernel
         * @param pMap
         * @param pRingBuffer
         */
        DeviceBuffer(const std::string& pName, const shape_t& pShapePacked, const std::size_t pMapSize, xrt::bo& pInternalBo, xrt::kernel& pAssociatedKernel, T* pMap, RingBuffer<T>& pRingBuffer)
            : name(pName), shapePacked(pShapePacked), mapSize(pMapSize), internalBo(std::move(pInternalBo)), associatedKernel(pAssociatedKernel), map(pMap), logger(Logger::getLogger()) {}

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

        /**
         * @brief Executes the associated xrt kernel
         *
         * @return ert_cmd_state
         */
        virtual ert_cmd_state execute(uint batchsize = 1) = 0;
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
        DeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked) : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked){};

        /**
         * @brief Run the kernel to input data from FPGA memory into the accelerator design
         *
         * @return true Success
         * @return false Fail
         */
        virtual bool run() = 0;

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
        DeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked) : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked){};

        /**
         * @brief Get timeout
         *
         * @return unsigned int
         */
        virtual unsigned int getMsExecuteTimeout() const = 0;
        /**
         * @brief Set timeout
         *
         * @param val
         */
        virtual void setMsExecuteTimeout(unsigned int val) = 0;
        /**
         * @brief Return stored data from storage
         *
         * @return Finn::vector<T>
         */
        virtual Finn::vector<T> getData() = 0;
        /**
         * @brief Start XRT kernel to read data from accelerator design into FPGA memory
         *
         * @param samples
         * @return ert_cmd_state
         */
        virtual ert_cmd_state read(unsigned int samples) = 0;

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
