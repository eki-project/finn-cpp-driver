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

    namespace detail {
        /**
         * @brief Wrapper that contains the ringbuffer used by Synchronous Input & Output Buffers
         *
         * @tparam T Type of the data stored in the ringbuffer
         */
        template<typename T>
        class SyncBufferWrapper {
             protected:
            /**
             * @brief Internal Ringbuffer used by all synchronous buffers
             *
             */
            RingBuffer<T, false> ringBuffer;

            /**
             * @brief Construct a new Sync Buffer Wrapper object
             *
             * @param ringBufferSizeFactor Number of batch elements that should be able to be stored
             * @param elementsPerPart Number of values per batch element
             */
            SyncBufferWrapper(unsigned int ringBufferSizeFactor, std::size_t elementsPerPart) : ringBuffer(RingBuffer<T, false>(ringBufferSizeFactor, elementsPerPart)) {
                if (ringBufferSizeFactor == 0) {
                    FinnUtils::logAndError<std::runtime_error>("DeviceBuffer of size 0 cannot be constructed!");
                }
                FINN_LOG(Logger::getLogger(), loglevel::info) << "[SyncDeviceBuffer] Max buffer size:" << ringBufferSizeFactor << "*" << elementsPerPart << "\n";
            }

            /**
             * @brief Destroy the Sync Buffer Wrapper object
             *
             */
            ~SyncBufferWrapper() = default;

            /**
             * @brief Construct a new Sync Buffer Wrapper object (Move constructor)
             *
             * @param buf
             */
            SyncBufferWrapper(SyncBufferWrapper&& buf) noexcept : ringBuffer(std::move(buf.ringBuffer)) {}

            /**
             * @brief Construct a new Sync Buffer Wrapper object (Deleted copy constructor)
             *
             * @param buf
             */
            SyncBufferWrapper(const SyncBufferWrapper& buf) noexcept = delete;

            /**
             * @brief Deleted move assignment operator
             *
             * @param buf
             * @return SyncBufferWrapper&
             */
            SyncBufferWrapper& operator=(SyncBufferWrapper&& buf) = delete;

            /**
             * @brief Deleted copy assignment operator
             *
             * @param buf
             * @return SyncBufferWrapper&
             */
            SyncBufferWrapper& operator=(const SyncBufferWrapper& buf) = delete;
#ifdef UNITTEST
             public:
            RingBuffer<T, false>& testGetRingBuffer() { return this->ringBuffer; }
#endif
        };
    }  // namespace detail

    template<typename T>
    class SyncDeviceInputBuffer : public DeviceInputBuffer<T> {
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
        SyncDeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int batchSize) : DeviceInputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked) {
            FINN_LOG(this->logger, loglevel::info) << "[SyncDeviceInputBuffer] "
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

         private:
        friend class DeviceInputBuffer<T>;

         public:
        size_t size(SIZE_SPECIFIER ss) override {
            switch (ss) {
                case SIZE_SPECIFIER::BYTES: {
                    return FinnUtils::shapeToElements(this->shapePacked) * sizeof(T);
                }
                case SIZE_SPECIFIER::FEATUREMAP_SIZE: {
                    return FinnUtils::shapeToElements(this->shapePacked) / this->shapePacked[0];
                }
                case SIZE_SPECIFIER::BATCHSIZE: {
                    return this->shapePacked[0];
                }
                case SIZE_SPECIFIER::TOTAL_DATA_SIZE: {
                    return FinnUtils::shapeToElements(this->shapePacked);
                }
                default:
                    return 0;
            }
        }

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
            FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
            this->sync(FinnUtils::shapeToElements(this->shapePacked));
            execute(this->shapePacked[0]);
            return true;
        }

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
        SyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int batchSize)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked) {
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
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) override {
            switch (ss) {
                case SIZE_SPECIFIER::BYTES: {
                    return FinnUtils::shapeToElements(this->shapePacked) * sizeof(T);
                }
                case SIZE_SPECIFIER::FEATUREMAP_SIZE: {
                    return FinnUtils::shapeToElements(this->shapePacked) / this->shapePacked[0];
                }
                case SIZE_SPECIFIER::BATCHSIZE: {
                    return this->shapePacked[0];
                }
                case SIZE_SPECIFIER::TOTAL_DATA_SIZE: {
                    return FinnUtils::shapeToElements(this->shapePacked);
                }
                default:
                    return 0;
            }
        }


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
         * @brief Return the data contained in the FPGA Buffer map.
         *
         * @return Finn::vector<T>
         */
        Finn::vector<T> getData() override {
            Finn::vector<T> tmp(this->map, this->map + FinnUtils::shapeToElements(this->shapePacked));
            return tmp;
        }

        /**
         * @brief Read the specified number of batchSize. If a read fails, immediately return. If all are successful, the kernel state of the last run is returned
         *
         * @param batchSize
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int batchSize) override {
            FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "Reading " << batchSize << " samples from the device";
            ert_cmd_state outExecuteResult = execute(batchSize);  // Return error if batchSize == 0
            if (outExecuteResult == ERT_CMD_STATE_ERROR || outExecuteResult == ERT_CMD_STATE_ABORT) {
                return outExecuteResult;
            }
            this->sync(FinnUtils::shapeToElements(this->shapePacked));
            return outExecuteResult;
        }

         protected:
        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        ert_cmd_state execute(uint batchsize = 1) override {
            auto run = this->associatedKernel(this->internalBo, batchsize);
            return run.wait(this->msExecuteTimeout);
        }
    };
}  // namespace Finn

#endif  // SYNCDEVICEBUFFERS
