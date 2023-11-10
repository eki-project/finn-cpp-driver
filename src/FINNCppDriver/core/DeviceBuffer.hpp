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

#include <FINNCppDriver/utils/FinnDatatypes.hpp>
#include <FINNCppDriver/utils/RingBuffer.hpp>
#include <boost/circular_buffer.hpp>
#include <cstdlib>
#include <limits>

#include "ert.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

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
        RingBuffer<T> ringBuffer;

         public:
        //* Normal Constructor
        DeviceBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : name(pName),
              shapePacked(pShapePacked),
              mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
              internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
              associatedKernel(pAssociatedKernel),
              map(internalBo.template map<T*>()),
              logger(Logger::getLogger()),
              ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize)) {
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] "
                                             << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << ", BUFFER SIZE: " << ringBufferSizeFactor
                                             << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
            if (ringBufferSizeFactor == 0) {
                FinnUtils::logAndError<std::runtime_error>("DeviceBuffer of size 0 cannot be constructed currently!");
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
              logger(Logger::getLogger()),
              ringBuffer(std::move(buf.ringBuffer)) {}

        DeviceBuffer(const DeviceBuffer& buf) noexcept = delete;

        virtual ~DeviceBuffer() { FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] Destructing DeviceBuffer " << name << "\n"; };

        DeviceBuffer& operator=(DeviceBuffer&& buf) = delete;
        DeviceBuffer& operator=(const DeviceBuffer& buf) = delete;

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        virtual size_t size(SIZE_SPECIFIER ss) {
            if (ss == SIZE_SPECIFIER::VALUES_PER_INPUT) {
                return FinnUtils::shapeToElements(this->shapePacked);
            }
            return this->ringBuffer.size(ss);
        }

        virtual std::string& getName() = 0;
        virtual shape_t& getPackedShape() = 0;
        virtual void sync() = 0;
        virtual ert_cmd_state execute() = 0;

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
            : name(pName), shapePacked(pShapePacked), mapSize(pMapSize), internalBo(std::move(pInternalBo)), associatedKernel(pAssociatedKernel), map(pMap), logger(Logger::getLogger()), ringBuffer(std::move(pRingBuffer)) {}

         private:
        virtual std::string loggerPrefix() = 0;
    };

    template<typename T>
    class SyncDeviceInputBuffer;

    template<typename T>
    class DeviceInputBuffer : public DeviceBuffer<T> {
         public:
        virtual bool run() = 0;
        virtual bool loadMap() = 0;

        template<typename InputIt>
        bool store(InputIt first, InputIt last) {
            // return static_cast<const D*>(this)->template storeImpl<InputIt>(first, last);
            auto ptr = dynamic_cast<SyncDeviceInputBuffer<T>*>(this);
            if (ptr) {
                return ptr->template storeImpl<InputIt>(first, last);
            } else {
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


        DeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};

         private:
        template<typename InputIt>
        static bool storeImpl(InputIt first, InputIt last) {
            return false;
        }

#ifdef UNITTEST
         public:
        Finn::vector<T> testGetMap() {
            Finn::vector<T> temp;
            for (size_t i = 0; i < this->mapSize; i++) {
                temp.push_back(this->map[i]);
            }
            return temp;
        }
        void testSyncBackFromDevice() { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE); }
        xrt::bo& testGetInternalBO() { return this->internalBo; }
        RingBuffer<T>& testGetRingBuffer() { return this->ringBuffer; }
#endif
    };

    template<typename T>
    class SyncDeviceInputBuffer : public DeviceInputBuffer<T> {
        std::mutex runMutex;
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;
        bool executeAutomaticallyHalfway = false;

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
        SyncDeviceInputBuffer(SyncDeviceInputBuffer&& buf) noexcept
            : DeviceInputBuffer<T>(buf.name, buf.shapePacked, buf.mapSize, buf.internalBo, buf.associatedKernel, buf.map, buf.ringBuffer),
              ioMode(buf.ioMode),
              executeAutomatically(buf.executeAutomatically),
              executeAutomaticallyHalfway(buf.executeAutomaticallyHalfway){};

        SyncDeviceInputBuffer(const SyncDeviceInputBuffer& buf) noexcept = delete;
        ~SyncDeviceInputBuffer() override = default;
        SyncDeviceInputBuffer& operator=(SyncDeviceInputBuffer&& buf) = delete;
        SyncDeviceInputBuffer& operator=(const SyncDeviceInputBuffer& buf) = delete;

         private:
        friend class DeviceInputBuffer<T>;
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        std::string loggerPrefix() override {
            std::string str = "[SyncDeviceInputBuffer - ";
            str += this->name;
            str += "] ";
            return str;
        }

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
            return this->ringBuffer.storeFast(first, last);
        }

         public:
        std::string& getName() override { return this->name; }
        shape_t& getPackedShape() override { return this->shapePacked; }

        /**
         * @brief Sync data from the map to the device.
         *
         */
        void sync() override { this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE); }

        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        ert_cmd_state execute() override {
            // TODO(bwintermann): Make batch_size changeable from 1
            auto runCall = this->associatedKernel(this->internalBo, 1);
            runCall.wait();
            return runCall.state();
        }

        /**
         * @brief Execute the first valid data that is found in the buffer. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() override {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
            std::lock_guard<std::mutex> guard(runMutex);
            if (!loadMap()) {
                return false;
            }
            sync();
            execute();
            return true;
        }

        /**
         * @brief Load data from the ring buffer into the memory map of the device.
         *
         * @attention Invalidates the data that was moved to map
         *
         * @return true
         * @return false
         */
        bool loadMap() override { return this->ringBuffer.read(this->map); }
    };

    template<typename T>
    class DeviceOutputBuffer : public DeviceBuffer<T> {
         protected:
        const IO ioMode = IO::OUTPUT;
        Finn::vector<T> longTermStorage;
        unsigned int msExecuteTimeout = 1000;

         public:
        DeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};

        virtual void allocateLongTermStorage(unsigned int expectedEntries) = 0;
        virtual unsigned int getMsExecuteTimeout() const = 0;
        virtual void setMsExecuteTimeout(unsigned int val) = 0;
        virtual void saveMap() = 0;
        virtual void archiveValidBufferParts() = 0;
        virtual Finn::vector<T> retrieveArchive() = 0;
        virtual void clearArchive() = 0;
        virtual ert_cmd_state read(unsigned int samples) = 0;

#ifdef UNITTEST
        std::vector<T> testGetMap() {
            std::vector<T> temp;
            for (size_t i = 0; i < this->mapSize; i++) {
                temp.push_back(this->map[i]);
            }
            return temp;
        }

        template<typename IteratorType>
        void testSetMap(IteratorType first, IteratorType last) {
            if (std::distance(first, last) != this->mapSize) {
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
        RingBuffer<T>& testGetRingBuffer() { return this->ringBuffer; }
        Finn::vector<uint8_t>& testGetLTS() { return longTermStorage; }
#endif
    };

    template<typename T>
    class SyncDeviceOutputBuffer : public DeviceOutputBuffer<T> {
         public:
        SyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked, ringBufferSizeFactor){};


         private:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        std::string loggerPrefix() override {
            std::string str = "[SyncDeviceOutputBuffer - ";
            str += this->name;
            str += "] ";
            return str;
        }

         public:
        std::string& getName() override { return this->name; }
        shape_t& getPackedShape() override { return this->shapePacked; }

        /**
         * @brief Reserve enough storage for the expectedEntries number of entries. Note however that because this is a vec of vecs, this only allocates memory for the pointers, not the data itself.
         *
         * @param expectedEntries
         */
        void allocateLongTermStorage(unsigned int expectedEntries) override { this->longTermStorage.reserve(expectedEntries * this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)); }

        /**
         * @brief Get the the kernel timeout in miliseconds
         *
         * @return unsigned int
         */
        unsigned int getMsExecuteTimeout() const override { return this->msExecuteTimeout; }

        /**
         * @brief Set the kernel timeout in miliseconds
         *
         * @param val
         */
        void setMsExecuteTimeout(unsigned int val) override { this->msExecuteTimeout = val; }

        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync() override { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE); }

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        ert_cmd_state execute() override {
            auto run = this->associatedKernel(this->internalBo, 1);
            run.wait(this->msExecuteTimeout);
            return run.state();
        }

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() override {
            //! Fix that if no space is available, the data will be discarded!
            this->ringBuffer.template store<T*>(this->map, this->mapSize);
        }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        void archiveValidBufferParts() override {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Archiving data from ring buffer to long term storage";
            static const std::size_t elementsCount = FinnUtils::shapeToElements(this->shapePacked);
            this->longTermStorage.reserve(this->longTermStorage.size() + this->ringBuffer.size(SIZE_SPECIFIER::PARTS) * elementsCount);
            Finn::vector<uint8_t> tmp;
            tmp.reserve(this->ringBuffer.size(SIZE_SPECIFIER::PARTS) * elementsCount);
            this->ringBuffer.readAllValidParts(std::back_inserter(tmp), elementsCount);
            std::copy(tmp.begin(), tmp.begin() + static_cast<long int>(tmp.size()), std::back_inserter(this->longTermStorage));
        }

        /**
         * @brief Return the archive.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> retrieveArchive() override {
            Finn::vector<T> tmp(this->longTermStorage);
            clearArchive();
            return tmp;
        }

        /**
         * @brief Clear the archive of all it's entries
         *
         */
        void clearArchive() override { this->longTermStorage.clear(); }

        /**
         * @brief Read the specified number of samples. If a read fails, immediately return. If all are successfull, the kernel state of the last run is returned
         *
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int samples) override {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Reading " << samples << " samples from the device";
            ert_cmd_state outExecuteResult = ERT_CMD_STATE_ERROR;  // Return error if samples == 0
            for (unsigned int i = 0; i < samples; i++) {
                outExecuteResult = execute();
                if (outExecuteResult == ERT_CMD_STATE_ERROR || outExecuteResult == ERT_CMD_STATE_ABORT) {
                    return outExecuteResult;
                }
                sync();
                saveMap();
                if (this->ringBuffer.isFull()) {
                    archiveValidBufferParts();
                }
            }
            return outExecuteResult;
        }
    };
}  // namespace Finn

#endif  // DEVICEBUFFER_H