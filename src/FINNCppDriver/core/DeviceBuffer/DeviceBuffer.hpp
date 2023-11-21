/**
 * @file DeviceBuffer.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Implements a wrapper to get data easier to and from the FPGAs
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DEVICEBUFFER_H
#define DEVICEBUFFER_H

#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/RingBuffer.hpp>
#include <boost/type_index.hpp>

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
        std::string name;
        shapePacked_t shapePacked;  // Packed shape (Type T): (1,2,3)
        size_t mapSize;             // Numbers of type T: When F has bitwidth 2, and T has bitwidth 8, the folded shape would be (1,2,10) and the packed (1,2,3) and thus 6
        xrt::bo internalBo;
        xrt::kernel associatedKernel;
        T* map;
        logger_type& logger;


    public:
        //* Normal Constructor
        DeviceBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked)
            : name(pName),
            shapePacked(pShapePacked),
            mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
            internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
            associatedKernel(pAssociatedKernel),
            map(internalBo.template map<T*>()),
            logger(Logger::getLogger()) {
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
            for (std::size_t i = 0; i < mapSize; ++i) {
                map[i] = 0;
            }
        }

        //* Move Constructor
        DeviceBuffer(DeviceBuffer&& buf) noexcept
            : name(std::move(buf.name)),
            shapePacked(std::move(buf.shapePacked)),
            mapSize(buf.mapSize),
            internalBo(std::move(buf.internalBo)),
            associatedKernel(std::move(buf.associatedKernel)),
            map(std::move(buf.map)),
            logger(Logger::getLogger()) {}

        DeviceBuffer(const DeviceBuffer& buf) noexcept = delete;

        virtual ~DeviceBuffer() { FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] Destructing DeviceBuffer " << name << "\n"; };

        DeviceBuffer& operator=(DeviceBuffer&& buf) = delete;
        DeviceBuffer& operator=(const DeviceBuffer& buf) = delete;

        virtual size_t size(SIZE_SPECIFIER ss) = 0;

        virtual std::string& getName() { return name; }
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
        virtual void sync(std::size_t bytes) = 0;
        virtual ert_cmd_state execute() = 0;
    };

    template<typename T>
    class SyncDeviceInputBuffer;
    template<typename T>
    class AsyncDeviceInputBuffer;

    template<typename T>
    class DeviceInputBuffer : public DeviceBuffer<T> {
    protected:
        const IO ioMode = IO::INPUT;

    public:
        DeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked) : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked) {};

        virtual bool run() = 0;

        template<typename InputIt>
        bool store(InputIt first, InputIt last) {
            // TODO(linusjun): benchmark and possibly replace iterator interface with span
            if (auto ptr = dynamic_cast<SyncDeviceInputBuffer<T>*>(this)) {
                return ptr->template storeImpl<InputIt>(first, last);
            }
            else if (auto ptr = dynamic_cast<AsyncDeviceInputBuffer<T>*>(this)) {
                return ptr->template storeImpl<InputIt>(first, last);
            }
            else {
                return false;
            }
        }

        /**
         * @brief Store the given vector of data in the ring buffer
         * @attention This function is NOT THREAD SAFE!
         *
         * @param data
         * @return true
         * @return false
         */
        bool store(const Finn::vector<T>& data) { return store(data.begin(), data.end()); }

    protected:
        /**
         * @brief Sync data from the map to the device.
         *
         */
        void sync(std::size_t bytes) override { this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE); }

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


    template<typename T>
    class DeviceOutputBuffer : public DeviceBuffer<T> {
    protected:
        const IO ioMode = IO::OUTPUT;
        Finn::vector<T> longTermStorage;
        unsigned int msExecuteTimeout = 1000;

    public:
        DeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked) : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked) {};

        virtual unsigned int getMsExecuteTimeout() const = 0;
        virtual void setMsExecuteTimeout(unsigned int val) = 0;
        virtual void archiveValidBufferParts() = 0;
        virtual Finn::vector<T> retrieveArchive() = 0;
        virtual ert_cmd_state read(unsigned int samples) = 0;

    protected:
        virtual void clearArchive() = 0;
        virtual void allocateLongTermStorage(unsigned int expectedEntries) = 0;
        virtual void saveMap() = 0;
        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync(std::size_t bytes) override { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE); }

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

#endif  // DEVICEBUFFER_H