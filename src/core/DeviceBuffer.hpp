#include <boost/circular_buffer.hpp>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Types.h"
#include "../utils/Logger.h"
#include "../utils/RingBuffer.hpp"

#include "xrt.h"
#include "xrt/xrt_bo.h"


// TODO(bwintermann): Replace ... with using DeviceBuffer<T,F>::...

namespace Finn {
    /**
     * @brief Parent class for DeviceBuffer objects.  
     * 
     * @tparam T Datatype in which the data is stored (e.g. uint8_t) 
     * @tparam F FINN-Datatype which should be stored as a composition of T values
     */
    template<typename T, typename F>
    class DeviceBuffer {
        protected:
        std::string name;
        size_t numbers;
        shape_t shape;
        size_t mapSize;
        xrt::bo internalBo;
        xrt::kernel& associatedKernel;
        T* map;
        logger_type& logger;
        RingBuffer<T> ringBuffer;
        
        public:
        DeviceBuffer(
            const std::string& pName,
            xrt::device& device,
            xrt::kernel& pAssociatedKernel,
            const shape_t& pShape,
            unsigned int ringBufferSizeFactor
        ) :
            name(pName),
            numbers(FinnUtils::shapeToElements(pShape)),
            shape(pShape),
            mapSize(F().template requiredElements<T>()),
            internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
            associatedKernel(pAssociatedKernel),
            map(internalBo.template map<T*>()),
            logger(Logger::getLogger()),
            ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize))
        {
            FINN_LOG(logger, loglevel::info) << "Initialized DeviceBuffer " << name << " (SHAPE: " << FinnUtils::shapeToString(pShape) << ", BUFFER SIZE: " << ringBufferSizeFactor << " inputs of the given shape)\n"; 
        }
    };

    /**
     * @brief A wrapper to efficiently put data onto an xrt::bo object. Internally uses a ring buffer to manage large data batches. 
     * This wrapper can be used in one of two ways: Manually or automatically. In manual mode you would use the methods to target specific ring buffer parts and execute them. This enables more finegrained management over how
     * where and when data is stored and exeuted. An example usage would be:
     * @code {.cpp}
     * auto myDB = DeviceInputBuffer<uint8_t, DatatypeUint<2>>(...);
     * if (!myDB.isPartValid(0)) {
     *      myDB.store(myData, 0);
     *      myDB.loadMap(0);
     *      myDB.sync();
     *      myDB.execute();
     * }
     * @endcode
     * This example would check that the data at index 0 is invalid and proceed to write data there and execute it.
     * 
     * The more automatic approach would be something like this:
     * @code {.cpp}
     * auto myDB = DeviceInputBuffer<uint8_t, DatatypeUint<2>>(...);
     * myDB.setExecuteAutomatically(true);
     * while (true) {
     *      myDB.store(myData[someIndex++]);
     * }
     * @endcode
     * This would store more and more data until the buffer is full, at which point the next part would always be executed and invalidated to make space for a new entry. Alternatively you could do something like this:
     * @code {.cpp}
     * auto myDB = DeviceInputBuffer<uint8_t, DatatypeUint<2>>(...);
     * myDB.setExecuteAutomatically(false);
     * 
     * while(true) {
     *      if (!myDB.isHeadValid()) {
     *          myDB.store(myData[someIndex++]); 
     *      }
     * }
     * @endcode
     * and in another thread
     * @code {.cpp}
     * while(true) {
     *      index_t opposite = myDB.getHeadOppositeIndex();
     *      if(myDB.isPartValid(opposite)) {
     *          myDB.loadMap(opposite);
     *          myDB.sync();
     *          myDB.execute();
     *      }
     * }
     * @endcode
     * This works because loadMap, sync and execute are all read-only.
     * 
     * 
     * 
     * @tparam T The datatype that the buffer uses to represent everything internally. This is the datatype that gets put on the FPGAs
     * @tparam F The FINN-datatype that represent the actual data. It is represented by one or multiple values of type T
     */
    template<typename T, typename F>
    class DeviceInputBuffer : DeviceBuffer<T,F> {
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;
        
        using DeviceBuffer<T,F>::DeviceBuffer;

        public:
        /**
         * @brief Set the Execute Automatically Flag. If set, as soon as the buffer head pointer reaches the last element, the first one in the buffer (first input) gets loaded and executed on the board, the slot gets invalidated and is thus free'd for the next store operation.
         * 
         * @param value 
         */
        void setExecuteAutomatically(bool value) {
            executeAutomatically = value;
        }
        
        /**
         * @brief Check whether the Execute Automatically Flag is set. 
         * 
         * @return true 
         * @return false 
         */
        bool isExecutedAutomatically() const {
            return executeAutomatically;
        }

        /**
         * @brief Sync data from the map to the device.  
         * 
         */
        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }

        /**
         * @brief Start a run on the associated kernel and wait for it's result.
         * @attention This method is blocking  
         * 
         */
        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        /**
         * @brief Loads the head part into the xrt::bo memory map to make it ready for sync. This invalidates the read part and moves the head pointer to the next part. 
         * 
         */
        void loadMap() {
            this->ringBuffer.read(this->map, this->mapSize);
        }

        /**
         * @brief Loads the passed part into the xrt::bo memory map to make it ready for sync. This does NOT change the head pointer. The validity gets set according to the passed newValidity value.
         * 
         * @param partIndex 
         * @param newValidity
         */
        void loadMap(index_t partIndex, bool newValidity) {
            this->ringBuffer.getPart(this->map, this->mapSize, partIndex, newValidity);
        }

        /**
         * @brief Check if the head part is valid 
         * 
         * @return true 
         * @return false 
         */
        bool isHeadValid() {
            return this->ringBuffer.isPartValid();
        }

        /**
         * @brief Check if the part indexed by partIndex is valid 
         * 
         * @param partIndex 
         * @return true 
         * @return false 
         */
        bool isPartValid(index_t partIndex) {
            return this->ringBuffer.isPartValid(partIndex);
        }

        /**
         * @brief Checks if the buffer is full (i.e. if all parts are valid - a ring buffer cannot be __full__ technically) 
         * 
         * @return true 
         * @return false 
         */
        bool isBufferFull() {
            return this->ringBuffer.isFull();
        }

        /**
         * @brief Get the index of the part on the "opposite" side of the ring buffer. The value is always ceil'd, so that the opposite of 3 in a 12-part buffer is 10, not 9. 
         * 
         * @return index_t 
         */
        index_t getHeadOpposideIndex() {
            return this->ringBuffer.getHeadOpposite();
        }

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values. 
         * 
         * @param ss 
         * @return size_t 
         */
        size_t size(SIZE_SPECIFIER ss) {
            return this->ringBuffer.size(ss);
        }

        /**
         * @name Store Methods 
         * The device buffer offers several options to store data inside it's buffer. There is a pair to read from an array, and a pair to read from a vector. 
         * In both cases, when no further argument is passed, the currently selected head part of the buffer is written to, and the head pointer is incremented, automatically filling up the buffer. If setExecuteAutomatically(true) was set, and the buffer is almost full, the last free element gets written, and the one after it gets executed and invalidated, to free one slot again. If the index_t partIndex was passed, the buffer at that index gets set, without the head pointer being changed. In both cases, the part that was just written gets set to "valid". It is strongly recommended to not break paradigm and use either all automatic storage/execution/reading or manual, but not both simultaneously. 
         */
        ///@{
        void store(const std::vector<T>& vec) {
            this->ringBuffer.store(vec);
            if (executeAutomatically && this->ringBuffer.isFull()) {
                loadMap();
                sync();
                execute();
            }
        }

        void store(const std::vector<T>& vec, index_t partIndex) {
            this->ringBuffer.setPart(vec, partIndex, true);
        }

        void store(const T& arr, const size_t arrSize) {
            this->ringBuffer.store(arr, arrSize);
            if (executeAutomatically && this->ringBuffer.isFull()) {
                loadMap();
                sync();
                execute();
            }
        }

        void store(const T& arr, const size_t arrSize, index_t partIndex) {
            this->ringBuffer.setPart(arr, arrSize, partIndex, true);
        }

        template<size_t sa>
        void store(const std::array<T,sa> arr) {
            store(arr.data(), arr.size());
        }

        template<size_t sa>
        void store(const std::array<T,sa> arr, index_t partIndex) {
            store(arr.data(), arr.size(), partIndex);
        }

        ///@}
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
    template<typename T, typename F>
    class DeviceOutputBuffer : DeviceBuffer<T,F> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;

        public:  
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
        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = this->associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        /**
         * @brief Store the contents of the memory map into the ring buffer. 
         * 
         */
        void saveMap() {
            this->ringBuffer.store(this->map, this->mapSize);
        }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. 
         * 
         */
        void archiveValidBufferParts() {
            for (index_t i = 0; i < this->ringBuffer.size(SIZE_SPECIFIER::PARTS); i++) {
                if (this->ringBuffer.isPartValid(i)) {
                    longTermStorage.push_back(this->ringBuffer.getPart(i, false));
                }
            }
        }

        /**
         * @brief Return the archive. 
         * 
         * @return std::vector<std::vector<T>> 
         */
        std::vector<std::vector<T>> retrieveArchive() const {
            return longTermStorage;
        }

        /**
         * @brief Clear the archive of all it's entries by resizing it to 0. 
         * 
         */
        void clearArchive() {
            longTermStorage.resize(0);
        }

        /**
         * @brief Read the specified number of samples from the device, only writing data into the archive when the ring buffer is full 
         * 
         * @param samples 
         */
        void read(unsigned int samples) {
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
} // namespace Finn