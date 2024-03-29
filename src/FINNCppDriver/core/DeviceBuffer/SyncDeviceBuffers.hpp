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
    class SyncDeviceInputBuffer : public DeviceInputBuffer<T>, public detail::SyncBufferWrapper<T> {
         public:
        /**
         * @brief Construct a new Sync Device Input Buffer object
         *
         * @param pName Name for indentification
         * @param device XRT device
         * @param pAssociatedKernel XRT kernel
         * @param pShapePacked packed shape of input
         * @param ringBufferSizeFactor size of ringbuffer in input elements (batch elements)
         */
        SyncDeviceInputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceInputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked), detail::SyncBufferWrapper<T>(ringBufferSizeFactor, FinnUtils::shapeToElements(pShapePacked)) {
            FINN_LOG(this->logger, loglevel::info) << "[SyncDeviceInputBuffer] "
                                                   << "Initializing DeviceBuffer " << this->name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << " inputs of the given shape, MAP SIZE: " << this->mapSize << ")\n";
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
         * @brief Load data from the ring buffer into the memory map of the device.
         *
         * @attention Invalidates the data that was moved to map
         *
         * @return true
         * @return false
         */
        bool loadMap() { return this->ringBuffer.read(this->map); }

         private:
        friend class DeviceInputBuffer<T>;

         public:
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
         * @return true
         * @return false
         */
        bool store(std::span<const T> data) override { return this->ringBuffer.store(data.begin(), data.end()); }

        /**
         * @brief Execute the first valid data that is found in the buffer. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() override {
            static const std::size_t elementCount = this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
            FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "DeviceBuffer (" << this->name << ") executing...";
            if (!loadMap()) {
                return false;
            }
            this->sync(elementCount);
            execute();
            return true;
        }

         protected:
        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        ert_cmd_state execute() override {
            auto runCall = this->associatedKernel(this->internalBo, 1);
            return runCall.wait();
        }
    };

    /**
     * @brief Implements a synchronous device buffer that transfers output data from the fpga to the host system
     *
     * @tparam T
     */
    template<typename T>
    class SyncDeviceOutputBuffer : public DeviceOutputBuffer<T>, public detail::SyncBufferWrapper<T> {
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
        SyncDeviceOutputBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : DeviceOutputBuffer<T>(pName, device, pAssociatedKernel, pShapePacked), detail::SyncBufferWrapper<T>(ringBufferSizeFactor, FinnUtils::shapeToElements(pShapePacked)){};

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
        size_t size(SIZE_SPECIFIER ss) override { return this->ringBuffer.size(ss); }


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
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        void archiveValidBufferParts() override {
            FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "Archiving data from ring buffer to long term storage";
            this->longTermStorage.reserve(this->longTermStorage.size() + this->ringBuffer.size());
            this->ringBuffer.readAllValidParts(std::back_inserter(this->longTermStorage));
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
         * @brief Read the specified number of samples. If a read fails, immediately return. If all are successfull, the kernel state of the last run is returned
         *
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int samples) override {
            static const std::size_t elementCount = this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART);
            FINN_LOG_DEBUG(logger, loglevel::info) << this->loggerPrefix() << "Reading " << samples << " samples from the device";
            ert_cmd_state outExecuteResult = ERT_CMD_STATE_ERROR;  // Return error if samples == 0
            for (unsigned int i = 0; i < samples; i++) {
                outExecuteResult = execute();
                if (outExecuteResult == ERT_CMD_STATE_ERROR || outExecuteResult == ERT_CMD_STATE_ABORT) {
                    return outExecuteResult;
                }
                this->sync(elementCount);
                saveMap();
                if (this->ringBuffer.full()) {
                    archiveValidBufferParts();
                }
            }
            return outExecuteResult;
        }

        /**
         * @brief Reserve enough storage for the expectedEntries number of entries. Note however that because this is a vec of vecs, this only allocates memory for the pointers, not the data itself.
         *
         * @param expectedEntries
         */
        void allocateLongTermStorage(unsigned int expectedEntries) override { this->longTermStorage.reserve(expectedEntries * this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)); }

#ifdef UNITTEST
         public:
#else
         protected:
#endif

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() override { this->ringBuffer.template store<T*>(this->map, this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)); }

         protected:
        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        ert_cmd_state execute() override {
            auto run = this->associatedKernel(this->internalBo, 1);
            return run.wait(this->msExecuteTimeout);
        }

        /**
         * @brief Clear the archive of all it's entries
         *
         */
        void clearArchive() override { this->longTermStorage.clear(); }
    };
}  // namespace Finn

#endif  // SYNCDEVICEBUFFERS
