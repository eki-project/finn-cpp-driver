/**
 * @file SyncDeviceBuffers.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements synchronous FPGA interfaces to transfer data to and from the FPGA.
 * @version 2.0
 * @date 2023-12-20
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef SYNCDEVICEBUFFERS
#define SYNCDEVICEBUFFERS

#include <FINNCppDriver/core/DeviceBuffer/DeviceBuffer.hpp>
#include <FINNCppDriver/utils/join.hpp>

#include "ert.h"

namespace Finn {
    template<typename T>
    class SyncDeviceInputBuffer : public DeviceInputBuffer<T> {
         private:
        friend class DeviceInputBuffer<T>;

         public:
        /**
         * @brief Construct a new Sync Device Input Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param batchSize batch size
         */
        SyncDeviceInputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize) : DeviceInputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize) {
            FINN_LOG(loglevel::info) << "[SyncDeviceInputBuffer] "
                                     << "Initializing DeviceBuffer " << this->name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << this->mapSize << ")\n";
            this->shapePacked[0] = batchSize;
        };

        /**
         * @brief Construct a new Sync Device Input Buffer object (Move constructor)
         *
         * @param buf
         */
        SyncDeviceInputBuffer(SyncDeviceInputBuffer&& buf) noexcept = default;

        /**
         * @brief Construct a new Sync Device Input Buffer object (Deleted copy constructor)
         *
         * @param buf
         */
        SyncDeviceInputBuffer(const SyncDeviceInputBuffer& buf) noexcept = delete;

        /**
         * @brief Destroy the Sync Device Input Buffer object
         *
         */
        ~SyncDeviceInputBuffer() override = default;

        /**
         * @brief Deleted move assignment operator
         *
         * @param buf
         * @return SyncDeviceInputBuffer&
         */
        SyncDeviceInputBuffer& operator=(SyncDeviceInputBuffer&& buf) = delete;

        /**
         * @brief Deleted copy assignment operator
         *
         * @param buf
         * @return SyncDeviceInputBuffer&
         */
        SyncDeviceInputBuffer& operator=(const SyncDeviceInputBuffer& buf) = delete;

#ifdef UNITTEST
         public:
#else
         protected:
#endif

        /**
         * @brief Store the given data in the input map of the FPGA
         *
         * @param data
         * @return true
         * @return false
         */
        bool store(std::span<const T> data) override {
            std::copy(data.begin(), data.end(), this->map);
            return true;
        }

        /**
         * @brief Execute the input kernel with the input stored in the input map. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() override {
            FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
            this->sync(FinnUtils::shapeToElements(this->shapePacked));
            this->execute(this->shapePacked[0]);
            return true;
        }
    };

    /**
     * @brief Implements a synchronous device buffer that transfers output data from the fpga to the host system
     *
     * @tparam T
     */
    template<typename T>
    class SyncDeviceOutputBuffer : public DeviceOutputBuffer<T> {
         public:
        /**
         * @brief Construct a new Synchronous Device Output Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param ringBufferSizeFactor size of ringbuffer in input elements (batch elements)
         */
        SyncDeviceOutputBuffer(const std::string& pCUName, xrt::device& device, xrt::uuid& pDevUUID, const shapePacked_t& pShapePacked, unsigned int batchSize) : DeviceOutputBuffer<T>(pCUName, device, pDevUUID, pShapePacked, batchSize) {
            this->shapePacked[0] = batchSize;
        };

        /**
         * @brief Construct a new Sync Device Output Buffer object (Move constructor)
         *
         * @param buf
         */
        SyncDeviceOutputBuffer(SyncDeviceOutputBuffer&& buf) noexcept = default;
        /**
         * @brief Construct a new Sync Device Output Buffer object (Deleted copy constructor)
         *
         * @param buf
         */
        SyncDeviceOutputBuffer(const SyncDeviceOutputBuffer& buf) noexcept = delete;
        /**
         * @brief Destroy the Sync Device Output Buffer object
         *
         */
        ~SyncDeviceOutputBuffer() override = default;
        /**
         * @brief Deleted move assignment operator
         *
         * @param buf
         * @return SyncDeviceOutputBuffer&
         */
        SyncDeviceOutputBuffer& operator=(SyncDeviceOutputBuffer&& buf) = delete;
        /**
         * @brief Deleted copy assignment operator
         *
         * @param buf
         * @return SyncDeviceOutputBuffer&
         */
        SyncDeviceOutputBuffer& operator=(const SyncDeviceOutputBuffer& buf) = delete;

        /**
         * @brief Return the data contained in the FPGA Buffer map.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> getData(const std::size_t& numItems) override {
            Finn::vector<T> tmp(this->map, this->map + numItems);
            return tmp;
        }

        /**
         * @brief Execute the output kernel.
         *
         * @return true
         * @return false
         */
        bool run() override {
            FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
            this->execute(this->shapePacked[0]);
            return true;
        }

        /**
         * @brief Read the specified number of batchSize. If a read fails, immediately return. If all are successful, the kernel state of the last run is returned
         *
         * @return bool
         */
        bool read() override {
            FINN_LOG_DEBUG(loglevel::info) << this->loggerPrefix() << "Synching  " << this->totalDataSize << " bytes from the device";
            this->sync(this->totalDataSize);
            return true;
        }
    };
}  // namespace Finn

#endif  // SYNCDEVICEBUFFERS
