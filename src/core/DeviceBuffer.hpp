#ifndef DEVICEBUFFER_H
#define DEVICEBUFFER_H


#include <boost/circular_buffer.hpp>
#include <limits>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Logger.h"
#include "../utils/RingBuffer.hpp"
#include "../utils/Types.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"


// TODO(bwintermann): Replace ... with using DeviceBuffer<T,F>::...

namespace Finn {
    /**
     * @brief Parent class for DeviceBuffer objects.
     *
     * @tparam T Datatype in which the data is stored (e.g. uint8_t)
     * @tparam F FINN-Datatype which should be stored as a composition of T values
     */
    template<typename T /*, typename F*/>
    class DeviceBuffer {
         protected:
        std::string name;
        // size_t numbers;  // Numbers of type F: in a shape (1,20) this would be 20
        //  shape_t shapeNormal;  // Input shape (Type F): (1,20)
        //  shape_t shapeFolded;  // Folded shape (Type F): (1,2,10)
        shapePacked_t shapePacked;  // Packed shape (Type T): (1,2,3)
        size_t mapSize;             // Numbers of type T: When F has bitwidth 2, and T has bitwidth 8, the folded shape would be (1,2,10) and the packed (1,2,3) and thus 6
        xrt::bo internalBo;
        xrt::kernel associatedKernel;
        T* map;
        logger_type& logger;
        RingBuffer<T> ringBuffer;

         public:
        DeviceBuffer(const std::string& pName, xrt::device& device, xrt::kernel pAssociatedKernel, /*const shape_t& pShapeNormal, const shape_t& pShapeFolded,*/ const shapePacked_t& pShapePacked, unsigned int ringBufferSizeFactor)
            : name(pName),
              // numbers(FinnUtils::shapeToElements(pShapeNormal)),
              //  shapeNormal(pShapeNormal),
              //  shapeFolded(pShapeFolded),
              shapePacked(pShapePacked),
              mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
              internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
              associatedKernel(pAssociatedKernel),
              map(internalBo.template map<T*>()),
              logger(Logger::getLogger()),
              ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize)) {
            FINN_LOG(logger, loglevel::info) << "Initializing DeviceBuffer " << name << " (SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << ", BUFFER SIZE: " << ringBufferSizeFactor
                                             << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n";
        }

        DeviceBuffer(DeviceBuffer&& buf) noexcept
            : name(std::move(buf.name)),
              shapePacked(std::move(buf.shapePacked)),
              mapSize(buf.mapSize),
              internalBo(std::move(buf.internalBo)),
              associatedKernel(buf.associatedKernel),
              map(std::move(buf.map)),
              logger(Logger::getLogger()),
              ringBuffer(std::move(buf.ringBuffer)) {}

        DeviceBuffer(const DeviceBuffer& buf) noexcept = delete;

        virtual ~DeviceBuffer() { FINN_LOG(logger, loglevel::info) << "Destructing DeviceBuffer " << name << "\n"; };

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

        std::string loggerPrefix() {
            std::string str = "[";
            str += this->name;
            str += "] ";
            return str;
        }
    };


    template<typename T /*, typename F*/>
    class DeviceInputBuffer : DeviceBuffer<T /*, F*/> {
        std::mutex runMutex;

        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;
        bool executeAutomaticallyHalfway = false;

        using DeviceBuffer<T /*, F*/>::DeviceBuffer;
        using DeviceBuffer<T /*, F*/>::logger;

         public:
        /**
         * @brief Move Constructor !!!NOT THREAD SAFE!!!
         *
         * @param buf
         */
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
            std::string str = "[INPUT - ";
            str += this->name;
            str += "] ";
            return str;
        }

#ifdef INSPECTION_TEST
         public:
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

         public:
        /**
         * @brief Sync data from the map to the device.
         *
         */
        void sync() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Syncing to device";
            this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }

        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking
         *
         */
        void execute() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Executing the kernel " << this->associatedKernel.get_name();
            // TODO(bwintermann): Make batch_size changeable from 1
            auto runCall = this->associatedKernel(this->internalBo, 1);
            runCall.wait();
        }

        bool loadMap() {
            FINN_LOG_DEBUG(logger, loglevel::debug) << loggerPrefix() << "Loading data from ring buffer into map";
            return this->ringBuffer.read(this->map, this->mapSize);
        }

        bool store(const std::vector<T>& data) {
            // TODO(bjarne): Enable support to write multiple parts from one vector, which has then to be a multiple of elementsPerPart large
            return this->ringBuffer.template store<std::vector<T>>(data, data.size());
        }

        template<typename VecUintIt>
        bool store(VecUintIt beginning, VecUintIt end) {
            static_assert(std::is_same<typename std::iterator_traits<VecUintIt>::value_type, T>::value);
            return this->ringBuffer.store(beginning, end);
        }

        bool run() {
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
    };


    /**
     * @brief DeviceBuffer for reading output data from the inference run.
     * Example usage:
     * @code {.cpp}
     * auto myDB = DeviceOutputBuffer<uint8_t, DatatypeUint<2>>(...);
     * for (int i = 0; i < 1000; i++) {
     *      myDB.read(100); // Read inferences
     * }
     * auto inferenceData = myDB.retrieveArchive();
     * myDB.clearArchive();
     * @endcode
     * This would read 100 samples at a time, 1000 times. The data gets read from the FPGA and stored in the internal ring buffer.
     * When the ring buffer is full, the data gets placed in the long term storage (archive), from which it can be read or cleared. In order to avoid performance hickups,
     * it is advised to make the ringBufferSizeFactor of the DeviceOutputBuffer a whole multiple of the batch size of your dataset, so that the longer copying of the ring buffer to the archive does not happen mid-batch-inference.
     *
     * @tparam T
     * @tparam F
     */
    template<typename T /*, typename F*/>
    class DeviceOutputBuffer : DeviceBuffer<T /*, F*/> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;

        using DeviceBuffer<T /*, F*/>::DeviceBuffer;
        using DeviceBuffer<T /*, F*/>::logger;

         private:
        /**
         * @brief Returns a device prefix for logging
         *
         * @return std::string
         */
        std::string loggerPrefix() {
            std::string str = "[OUTPUT - ";
            str += this->name;
            str += "] ";
            return str;
        }

#ifdef INSPECTION_TEST
         public:
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

         public:
        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Syncing data from device";
            this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        void execute() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Executing on device";
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = this->associatedKernel(this->internalBo, 1);
            run.wait();
        }

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Saving data from device map into ring buffer";
            this->ringBuffer.template store<T*>(this->map, this->mapSize);
        }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full.
         *
         */
        // TODO(bjarne): Make more efficient
        void archiveValidBufferParts() {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Archiving data from ring buffer to long term storage";
            for (index_t i = 0; i < this->ringBuffer.size(SIZE_SPECIFIER::PARTS); i++) {
                longTermStorage.push_back(std::vector<T>(this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART)));
                this->ringBuffer.read(longTermStorage.back(), this->ringBuffer.size(SIZE_SPECIFIER::ELEMENTS_PER_PART));
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
         * @brief Read the specified number of samples from the device, only writing data into the archive when the ring buffer is full
         *
         * @param samples
         */
        void read(unsigned int samples) {
            FINN_LOG_DEBUG(logger, loglevel::info) << loggerPrefix() << "Reading " << samples << " samples from the device";
            for (unsigned int i = 0; i < samples; i++) {
                execute();
                sync();
                saveMap();
                if (this->ringBuffer.isFull()) {
                    archiveValidBufferParts();
                }
            }
        }
    };

    template<typename T /*, typename F*/>
    DeviceInputBuffer<T /*, F*/> makeAutomaticInputBuffer(const std::string& name, const xrt::device& device, const xrt::kernel& kern, /*const shape_t& shapeNormal, const shape_t& shapeFolded,*/ const shape_t& shapePacked,
                                                          unsigned int bufferSize) {
        auto tmp = DeviceInputBuffer<T /*, F*/>(name, device, kern, /*shapeNormal, shapeFolded,*/ shapePacked, bufferSize);
        tmp.setExecuteAutomatically(true);
        tmp.setExecuteAutomaticallyHalfway(true);
        return tmp;
    }

    template<typename T /*, typename F*/>
    DeviceInputBuffer<T /*, F*/> makeManualInputBuffer(const std::string& name, const xrt::device& device, const xrt::kernel& kern, /*const shape_t& shapeNormal, const shape_t& shapeFolded,*/ const shape_t& shapePacked,
                                                       unsigned int bufferSize) {
        auto tmp = DeviceInputBuffer<T /*, F*/>(name, device, kern, /*shapeNormal, shapeFolded,*/ shapePacked, bufferSize);
        tmp.setExecuteAutomatically(false);
        tmp.setExecuteAutomaticallyHalfway(false);
        return tmp;
    }
}  // namespace Finn

#endif  // DEVICEBUFFER_H