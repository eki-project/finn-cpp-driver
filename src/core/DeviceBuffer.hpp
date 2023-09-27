#ifndef DEVICEBUFFER_H
#define DEVICEBUFFER_H

#include <boost/circular_buffer.hpp>
#include <cstdlib>
#include <limits>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Logger.h"
#include "../utils/RingBuffer.hpp"
#include "../utils/Types.h"
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
        DeviceBuffer(const std::string& pName, xrt::device& device, xrt::kernel& pAssociatedKernel, /*const shape_t& pShapeNormal, const shape_t& pShapeFolded,*/ const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : name(pName),
              shapePacked(pShapePacked),
              mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
              internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
              associatedKernel(pAssociatedKernel),
              map(internalBo.template map<T*>()),
              logger(Logger::getLogger()),
              ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize)) {
            FINN_LOG(logger, loglevel::info) << "[DeviceBuffer] " << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << ", BUFFER SIZE: " << ringBufferSizeFactor
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
    };


    template<typename T>
    class DeviceInputBuffer : DeviceBuffer<T> {
        std::mutex runMutex;
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;
        bool executeAutomaticallyHalfway = false;
        using DeviceBuffer<T>::DeviceBuffer;
        using DeviceBuffer<T>::logger;

         public:
        std::string& getName() { return this->name; }
        shape_t& getPackedShape() { return this->shapePacked; }
        /**
         * @brief Move Constructor
         * @attention This move constructor is NOT THREAD SAFE
         *
         * @param buf
         */
        //! NOT THREAD SAFE
        DeviceInputBuffer(DeviceInputBuffer&& buf) noexcept
            : DeviceBuffer<T>(buf.name, buf.shapePacked, buf.mapSize, buf.internalBo, buf.associatedKernel, buf.map, buf.ringBuffer),
              ioMode(buf.ioMode),
              executeAutomatically(buf.executeAutomatically),
              executeAutomaticallyHalfway(buf.executeAutomaticallyHalfway){};

        DeviceInputBuffer(const DeviceInputBuffer& buf) noexcept = delete;
        ~DeviceInputBuffer() override = default;
        DeviceInputBuffer& operator=(DeviceInputBuffer&& buf) = delete;
        DeviceInputBuffer& operator=(const DeviceInputBuffer& buf) = delete;

         private:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        std::string loggerPrefix() {
            std::string str = "[DeviceInputBuffer - ";
            str += this->name;
            str += "] ";
            return str;
        }

         public:
        /**
         * @brief Sync data from the map to the device.
         *
         */
        void sync() { this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE); }

        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        void execute() {
            // TODO(bwintermann): Make batch_size changeable from 1
            auto runCall = this->associatedKernel(this->internalBo, 1);
            runCall.wait();
        }

        /**
         * @brief Load data from the ring buffer into the memory map of the device
         *
         * @return true
         * @return false
         */
        bool loadMap() { return this->ringBuffer.read(this->map, this->mapSize); }

        /**
         * @brief Store the given vector of data in the ring buffer
         *
         * @param data
         * @return true
         * @return false
         */
        bool store(const std::vector<T>& data) {
            // TODO(bwintermann): Enable support to write multiple parts from one vector, which has then to be a multiple of elementsPerPart large
            return this->ringBuffer.template store<std::vector<T>>(data, data.size());
        }

        /**
         * @brief Store the given data in the ring buffer
         *
         * @tparam InputIt The type of the iterator
         * @param beginning
         * @param end
         * @return true
         * @return false
         */
        template<typename InputIt>
        bool store(InputIt beginning, InputIt end) {
            static_assert(std::is_same<typename std::iterator_traits<InputIt>::value_type, T>::value);
            return this->ringBuffer.store(beginning, end);
        }

        /**
         * @brief Execute the first valid data that is found in the buffer. Returns false if no valid data was found
         *
         * @return true
         * @return false
         */
        bool run() {
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
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) { return this->ringBuffer.size(ss); }


#ifndef NDEBUG
        std::vector<T> testGetMap() {
            std::vector<T> temp;
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
    class DeviceOutputBuffer : DeviceBuffer<T> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;
        unsigned int msExecuteTimeout = 1000;

        using DeviceBuffer<T>::DeviceBuffer;
        using DeviceBuffer<T>::logger;

         private:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        std::string loggerPrefix() {
            std::string str = "[DeviceOutputBuffer - ";
            str += this->name;
            str += "] ";
            return str;
        }

         public:
        std::string& getName() { return this->name; }
        shape_t& getPackedShape() { return this->shapePacked; }
        /**
         * @brief Get the the kernel timeout in miliseconds
         *
         * @return unsigned int
         */
        unsigned int getMsExecuteTimeout() const { return msExecuteTimeout; }

        /**
         * @brief Set the kernel timeout in miliseconds
         *
         * @param val
         */
        void setMsExecuteTimeout(unsigned int val) { msExecuteTimeout = val; }

        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        ert_cmd_state execute() {
            auto run = this->associatedKernel(this->internalBo, 1);
            run.wait(msExecuteTimeout);
            return run.state();
        }

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() {
            //! Fix that if no space is available, the data will be discarded!
            this->ringBuffer.template store<T*>(this->map, this->mapSize);
        }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        //? This should be done more efficiently maybe?
        //? Implement a variant for iterators?
        void archiveValidBufferParts() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Archiving data from ring buffer to long term storage";
            for (index_t i = 0; i < this->ringBuffer.size(SIZE_SPECIFIER::PARTS); i++) {
                auto tmp = std::vector<T>();
                tmp.resize(this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
                auto dataStored = this->ringBuffer.read(tmp, this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
                if (dataStored) {
                    longTermStorage.push_back(tmp);
                }
            }
        }

        /**
         * @brief Return the archive.
         *
         * @return std::vector<std::vector<T>>
         */
        std::vector<std::vector<T>> retrieveArchive() const { return longTermStorage; }

        /**
         * @brief Clear the archive of all it's entries by resizing it to 0.
         *
         */
        void clearArchive() { longTermStorage.resize(0); }

        /**
         * @brief Read the specified number of samples. If a read fails, immediately return. If all are successfull, the kernel state of the last run is returned
         *
         * @param samples
         * @return ert_cmd_state
         */
        ert_cmd_state read(unsigned int samples) {
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

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) { return this->ringBuffer.size(ss); }


#ifndef NDEBUG
        std::vector<T> testGetMap() {
            std::vector<T> temp;
            for (size_t i = 0; i < this->map_size; i++) {
                temp.push_back(this->map[i]);
            }
            return temp;
        }
        unsigned int testGetLongTermStorageSize() const { return longTermStorage.size(); }
        xrt::bo& testGetInternalBO() { return this->interalBo; }
        RingBuffer<T>& testGetRingBuffer() { return this->ringBuffer; }
#endif
    };
}  // namespace Finn

#endif  // DEVICEBUFFER_H