#include <boost/circular_buffer.hpp>
#include <magic_enum.hpp>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Types.h"
#include "../utils/Logger.h"
#include "../utils/RingBuffer.hpp"

#include "xrt.h"
#include "xrt/xrt_bo.h"


// TODO(bwintermann): Replace this->... with using DeviceBuffer<T,F>::...

namespace Finn {
    /**
     * @brief Parent class for DeviceBuffer objects.  
     * 
     * @tparam T Datatype in which the data is stored (e.g. uint8_t) 
     * @tparam F FINN-Datatype which should be stored as a composition of T values
     */
    template<typename T, typename F>
    class DeviceBuffer {
        const std::string name;
        const size_t numbers;
        const shape_t shape;

        xrt::bo internalBo;
        xrt::kernel& associatedKernel;
        
        T* map;
        const size_t mapSize;

        RingBuffer<T> ringBuffer;

        DeviceBuffer(
            const std::string& pName,
            xrt::device& device,
            xrt::kernel& pAssociatedKernel,
            const shape_t pShape,
            unsigned int ringBufferSizeFactor
        ) : name(pName),
            numbers(FinnUtils::shapeToElements(pShape)),
            shape(pShape),
            mapSize(F().template requiredElements<T>()),
            internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
            associatedKernel(pAssociatedKernel),
            map(internalBo.map<T*>()),
            ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize)) 
        {

        }

        virtual void sync();
        virtual void execute();

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
     * @tparam T 
     * @tparam F 
     */
    template<typename T, typename F>
    class DeviceInputBuffer : DeviceBuffer<T,F> {
        using DeviceBuffer<T,F>::DeviceBuffer;
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;

        public:
        void setExecuteAutomatically(bool value) {
            executeAutomatically = value;
        }
        
        bool isExecutedAutomatically() {
            return executeAutomatically;
        }

        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }

        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        void loadMap() {
            this->ringBuffer.read(this->map, this->mapSize);
        }

        void loadMap(index_t partIndex) {
            this->ringBuffer.getPart(this->map, this->mapSize, partIndex, false);
        }

        bool isHeadValid() {
            return this->ringBuffer.isPartValid();
        }

        bool isPartValid(index_t partIndex) {
            return this->ringBuffer.isPartValid(partIndex);
        }

        bool isBufferFull() {
            return this->ringBuffer.isFull();
        }

        index_t getHeadOpposideIndex() {
            return this->ringBuffer.getHeadOpposite();
        }

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
        ///@}
    };


    template<typename T, typename F>
    class DeviceOutputBuffer : DeviceBuffer<T,F> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;

        public:    
        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }

        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        void saveMap() {
            this->ringBuffer.store(this->map, this->mapSize);
        }

        void archiveValidBufferParts() {
            for (index_t i = 0; i < this->ringBuffer.size(SIZE_SPECIFIER::PARTS); i++) {
                if (this->ringBuffer.isPartValid(i)) {
                    longTermStorage.push_back(this->ringBuffer.getPart(i, false));
                }
            }
        }

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