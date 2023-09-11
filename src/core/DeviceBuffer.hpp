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
    template<typename T, typename F>
    class DeviceBuffer {
        protected:
        std::string name;
        size_t numbers;             // Numbers of type F: in a shape (1,20) this would be 20 
        shape_t shapeNormal;        // Input shape (Type F): (1,20)
        shape_t shapeFolded;        // Folded shape (Type F): (1,2,10)
        shape_t shapePacked;        // Packed shape (Type T): (1,2,3)
        size_t mapSize;             // Numbers of type T: When F has bitwidth 2, and T has bitwidth 8, the folded shape would be (1,2,10) and the packed (1,2,3) and thus 6
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
            const shape_t& pShapeNormal,
            const shape_t& pShapeFolded,
            const shape_t& pShapePacked,
            unsigned int ringBufferSizeFactor
        ) :
            name(pName),
            numbers(FinnUtils::shapeToElements(pShapeNormal)),
            shapeNormal(pShapeNormal),
            shapeFolded(pShapeFolded),
            shapePacked(pShapePacked),
            mapSize(FinnUtils::getActualBufferSize(FinnUtils::shapeToElements(pShapePacked))),
            internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
            associatedKernel(pAssociatedKernel),
            map(internalBo.template map<T*>()),
            logger(Logger::getLogger()),
            ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize))
        {
            // The following line calculates the new innermost dimension needed to represent the previous innermost dimension as type T's
            unsigned int calculatedInnermostDimension = static_cast<unsigned int>(
                F().bitwidth() * FinnUtils::innermostDimension(pShapeFolded) / (sizeof(T) * 8)
            ) + 1;
            
            FINN_LOG(logger, loglevel::info) << "Initializing DeviceBuffer " << name << " (SHAPE: " << FinnUtils::shapeToString(pShapeNormal) << ", SHAPE FOLDED: " << FinnUtils::shapeToString(pShapeFolded) << ", SHAPE PACKED: " << FinnUtils::shapeToString(pShapePacked) << ", BUFFER SIZE: " << ringBufferSizeFactor << " inputs of the given shape, MAP SIZE: " << mapSize << ")\n"; 

            if (FinnUtils::shapeToElements(pShapeNormal) != FinnUtils::shapeToElements(pShapeFolded)) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_normal and shape_folded should amount to the same number of elements!");
            }

            if (FinnUtils::innermostDimension(pShapePacked) != calculatedInnermostDimension) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_packed's innermost dimension in " + FinnUtils::shapeToString(pShapePacked) + " does not equal the calculated innermost dimension " + std::to_string(calculatedInnermostDimension));
            }
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
    class DeviceInputBuffer : DeviceBuffer<T, F> {
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;
        bool executeAutomaticallyHalfway = false;

        using DeviceBuffer<T,F>::DeviceBuffer;

        public:
        /**
         * @brief Set the Execute Automatically Flag. If set, as soon as the buffer head pointer reaches the last element, the first one in the buffer (first input) gets loaded and executed on the board, the slot gets invalidated and is
         * thus free'd for the next store operation.
         *
         * @param value
         */
        void setExecuteAutomatically(bool value) { executeAutomatically = value; }

        /**
         * @brief Set the Execute Automatically Halfway flag. When set, the buffer automatically starts executing on the buffer, as soon as the previous parts/2 parts are valid data 
         * 
         * @param value 
         */
        void setExecuteAutomaticallyHalfway(bool value) { executeAutomaticallyHalfway = value; }

        /**
         * @brief Check whether the Execute Automatically Flag is set.
         *
         * @return true
         * @return false
         */
        bool isExecutedAutomatically() const { return executeAutomatically; }

        /**
         * @brief Check whether the Execute Automatically Halfway Flag is set.
         * 
         * @return true 
         * @return false 
         */
        bool isExecutedAutomaticallyHalfway() const { return executeAutomaticallyHalfway; }

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
            // TODO(bwintermann): Add arguments for kernel run!
            // TODO(bwintermann): Make batch_size changeable from 1
            auto run = this->associatedKernel(this->internalBo, 1);
            
            // run.start() only necessary on 2nd run. Producing  a new run every time might cost cycles?
            //run.start();
            
            run.wait();
        }

        /**
         * @brief This function takes a partIndex and loads it into the memory map, syncs it to the device and executes it.
         * 
         * @param partIndex 
         * @param failOnInvalidData If set, the function errors if the part that was tried to be executed was invalid
         * @param invalidateDataAfterExecute Whether or not to invalidate data after using it. Default true.
         */
        void loadAndExecute(index_t partIndex, bool failOnInvalidData, bool invalidateDataAfterExecute = true) {
            if (failOnInvalidData && !this->ringBuffer.isPartValid(partIndex)) {
                FinnUtils::logAndError<std::runtime_error>("Tried loading and executing a buffer part which was marked as invalid data (not written yet or already used)!");
            }
            loadMap(partIndex, !invalidateDataAfterExecute);
            sync();
            execute();
        }

        /**
         * @brief Loads the head part into the xrt::bo memory map to make it ready for sync. This invalidates the read part and moves the head pointer to the next part.
         *
         */
        void loadMap() { this->ringBuffer.read(this->map, this->mapSize); }

        /**
         * @brief Loads the passed part into the xrt::bo memory map to make it ready for sync. This does NOT change the head pointer. The validity gets set according to the passed newValidity value.
         *
         * @param partIndex
         * @param newValidity
         */
        void loadMap(index_t partIndex, bool newValidity) { this->ringBuffer.getPart(this->map, this->mapSize, partIndex, newValidity); }

        /**
         * @brief Check if the head part is valid
         *
         * @return true
         * @return false
         */
        bool isHeadValid() { return this->ringBuffer.isPartValid(); }

        /**
         * @brief Check if the part indexed by partIndex is valid
         *
         * @param partIndex
         * @return true
         * @return false
         */
        bool isPartValid(index_t partIndex) { return this->ringBuffer.isPartValid(partIndex); }

        /**
         * @brief Checks if the buffer is full (i.e. if all parts are valid - a ring buffer cannot be __full__ technically)
         *
         * @return true
         * @return false
         */
        bool isBufferFull() { return this->ringBuffer.isFull(); }

        /**
         * @brief Get the index of the part on the "opposite" side of the ring buffer. The value is always ceil'd, so that the opposite of 3 in a 12-part buffer is 10, not 9.
         * @attention This should not be needed by the user. Only use this information if you know what to do with it.
         *
         * @return index_t
         */
        index_t getHeadOpposideIndex() const { return this->ringBuffer.getHeadOpposite(); }

        /**
         * @brief Get the head part index.
         * @attention This should not be needed by the user. Only use this information if you know what to do with it.
         * 
         * @return index_t 
         */
        index_t getHeadIndex() const { return this->ringBuffer.getHeadIndex(); }

        /**
         * @brief Return the size of the buffer as specified by the argument. Bytes returns all bytes the buffer takes up, elements returns the number of T-values, numbers the number of F-values.
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) { return this->ringBuffer.size(ss); }

        // TODO: Update docs
        /**
         * @name Store Methods
         * The device buffer offers several options to store data inside it's buffer. There is a pair to read from an array, and a pair to read from a vector.
         * In both cases, when no further argument is passed, the currently selected head part of the buffer is written to, and the head pointer is incremented, automatically filling up the buffer. If setExecuteAutomatically(true) was set,
         * and the buffer is almost full, the last free element gets written, and the one after it gets executed and invalidated, to free one slot again. If the index_t partIndex was passed, the buffer at that index gets set, without the
         * head pointer being changed. In both cases, the part that was just written gets set to "valid". It is strongly recommended to not break paradigm and use either all automatic storage/execution/reading or manual, but not both
         * simultaneously.
         */
        ///@{
        
        /**
         * @brief Store the passed data in the ring buffer. Advance the head pointer. Set the newly written data part to valid. 
         * @note This operation is thread safe. 
         * 
         * @param vec The data to write
         * @param overwriteValidData If true, the data only gets written at the head index, if the currently marked head part is invalid data.
         * @return true 
         * @return false 
         */
        bool store(const std::vector<T>& vec, bool overwriteValidData) {
            if (!overwriteValidData && this->ringBuffer.isPartValid(getHeadIndex())) {
                return false;
            }
            this->ringBuffer.store(vec);
            if (executeAutomatically && (this->ringBuffer.isFull() || this->ringBuffer.isPartValid(getHeadIndex()))) {
                loadAndExecute(getHeadIndex(), true, true);
            } else if (executeAutomaticallyHalfway && this->ringBuffer.isPreviousHalfValid()) {
                loadAndExecute(getHeadOpposideIndex(), true, true);
            }
            return true;
        }

        bool store(const std::vector<T>& vec, index_t partIndex, bool overwriteValidData) {
            if (!overwriteValidData && this->ringBuffer.isPartValid(getHeadIndex())) {
                return false;
            }
            this->ringBuffer.setPart(vec, partIndex, true);
            return true;
        }

        template<size_t sa>
        bool store(const std::array<T, sa>& arr, bool overwriteValidData) {
            if (!overwriteValidData && this->ringBuffer.isPartValid(getHeadIndex())) {
                return false;
            }
            this->ringBuffer.template store<sa>(arr);
            if (executeAutomatically && (this->ringBuffer.isFull() || this->ringBuffer.isPartValid(getHeadIndex()))) {
                loadAndExecute(getHeadIndex(), true, true);
            } else if (executeAutomaticallyHalfway && this->ringBuffer.isPreviousHalfValid()) {
                loadAndExecute(getHeadOpposideIndex(), true, true);
            }
            return true;
        }

        template<size_t sa>
        bool store(const std::array<T, sa>& arr, index_t partIndex, bool overwriteValidData) {
            if (!overwriteValidData && this->ringBuffer.isPartValid(getHeadIndex())) {
                return false;
            }
            this->ringBuffer.setPart<sa>(arr, partIndex);
            return true;
        }
        ///@}

        // TODO(bwintermann): Exchange this for the operator[] at some time
        /**
         * @brief Get the selected part as an array. 
         * @attention To be replaced by the operator[] 
         * @attention Should not be used directly by the user
         * 
         * @param partIndex 
         * @return * template<size_t S> 
         */
        template<size_t S>
        std::array<T,S> get(index_t partIndex) {
            return this->ringBuffer.template getPart<S>(partIndex, isPartValid(partIndex));
        }

        std::vector<T> get(index_t partIndex) {
            return this->ringBuffer.template getPart(partIndex, isPartValid(partIndex));
        }
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
    class DeviceOutputBuffer : DeviceBuffer<T, F> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;

        using DeviceBuffer<T,F>::DeviceBuffer;
        
        public:  
        /**
         * @brief Sync data from the FPGA into the memory map
         *
         * @return * void
         */
        void sync() { this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE); }

        /**
         * @brief Execute the kernel and await it's return.
         * @attention This function is blocking.
         *
         */
        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = this->associatedKernel(this->internalBo);
            run.wait();
        }

        /**
         * @brief Store the contents of the memory map into the ring buffer.
         *
         */
        void saveMap() { this->ringBuffer.store(this->map, this->mapSize); }

        /**
         * @brief Put every valid read part of the ring buffer into the archive. This invalides them so that they are not put into the archive again.
         * @note After the function is executed, all parts are invalid.
         * @note This function can be executed manually instead of wait for it to be called by read() when the ring buffer is full. 
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

    template<typename T, typename F>
    DeviceInputBuffer<T,F> makeAutomaticInputBuffer(const std::string& name, const xrt::device& device, const xrt::kernel& kern, const shape_t& shapeNormal, const shape_t& shapeFolded, const shape_t& shapePacked, unsigned int bufferSize) {
        auto tmp = DeviceInputBuffer<T,F>(name, device, kern, shapeNormal, shapeFolded, shapePacked, bufferSize);
        tmp.setExecuteAutomatically(true);
        tmp.setExecuteAutomaticallyHalfway(true);
        return tmp;
    }

    template<typename T, typename F>
    DeviceInputBuffer<T,F> makeManualInputBuffer(const std::string& name, const xrt::device& device, const xrt::kernel& kern, const shape_t& shapeNormal, const shape_t& shapeFolded, const shape_t& shapePacked, unsigned int bufferSize) {
        auto tmp = DeviceInputBuffer<T,F>(name, device, kern, shapeNormal, shapeFolded, shapePacked, bufferSize);
        tmp.setExecuteAutomatically(false);
        tmp.setExecuteAutomaticallyHalfway(false);
        return tmp;
    }
}  // namespace Finn