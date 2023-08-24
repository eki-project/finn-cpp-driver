#include "../utils/FinnDatatypes.hpp"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include <boost/circular_buffer.hpp>
#include "../utils/Types.h"

/**
 * @brief 
 * 
 * @tparam T The smallest unit of data that a device manager has to manage 
 * @tparam B The bitwidth of the original FINN datatype
 */
template<typename T, typename B>
class DeviceBuffer {
    const IO bufferIOMode;
    xrt::bo _bo;
    T* map;
    const size_t sizeBytes; //sizeof(T) * elements
    const unsigned int sizeElements;
    const unsigned int sizeNumbers
    const Datatype<B> finnDatatype;
    const shape_t* bufferShape;
    
    boost::circular_buffer<T> ringBuffer;
    const unsigned int ringBufferSizeElements;    //! Size of ALL elements, not per active part
    const unsigned int ringBufferActiveParts;
    const unsigned int _ringBufferIndex;

    public:
    DeviceBuffer(xrt::device& device, const shape_t* pShape, unsigned int ringBufferSizeFactor, IO pBufferIOMode) : bufferShape(pShape), bufferIOMode(pBufferIOMode) {
        sizeNumbers = static_cast<unsigned int>(std::accumulate(pShape->begin(), pShape->end(), 1, std::multiplies<>()));
        sizeElements = sizeNumbers * finnDatatype.getRequiredElementsPerNumber(sizeof(T));
        sizeBytes = sizeof(T) * sizeElements;
        
        _bo = xrt::bo(device, sizeBytes, 0);        // TODO(bwintermann): Check if xrt::bo really takes bytes and not elements!
        map = _bo.map<T*>();

        ringBufferSizeElements = sizeElements * ringBufferSizeFactor;
        ringBufferActiveParts = ringBufferSizeFactor;
        ringBuffer = boost::circular_buffer<T>(ringBufferSizeElements);
        _ringBufferIndex = 0;

        static_assert(ringBuffer.size() == ringBuffer.capacity());
    }

    const bool isInputBuffer() { return bufferIOMode == IO::INPUT };
    const bool isOutputBuffer() { return bufferIOMode == IO::OUTPUT };
    const bool isInOutBuffer() { return bufferIOMode == IO::INOUT };

    /**
     * @brief Return the size of the buffer / map. Specifying BYTES returns the number of bytes, NUMBERS how many numbers the buffer has to store for one sample, ELEMENTS how many T's are required to store all NUMBERS and SAMPLE 1.  
     * 
     * @param ss The type of size 
     * @return unsigned int 
     */
    const unsigned int size(SIZE_SPECIFIER ss) {
        if (ss == SIZE_SPECIFIER::BYTES) {
            return static_cast<unsigned int>(sizeBytes);
        } else if (ss == SIZE_SPECIFIER::ELEMENTS) {
            return sizeElements;
        } else if (ss == SIZE_SPECIFIER::NUMBERS) {
            return sizeNumbers;
        } else if (ss == SIZE_SPECIFIER::SAMPLES) {
            return 1;
        } else {
            throw std::runtime_error("Unknown size specifier!");
        }
    }

    /**
     * @brief In this overloaded default size() function, the number of active parts of the ring buffer is returned.
     * This method should be used when the DeviceBuffer is treated like an array  
     * 
     * @return unsigned int 
     */
    const unsigned int size() {
        return ringBufferActiveParts;
    }

    /**
     * @brief Get the index of the current active buffer part! This does NOT return the actual ring buffer index, because that indexes elements, not active parts! 
     * 
     * @return unsigned int 
     */
    const unsigned int getCurrentIndex() {
        return static_cast<unsigned int>(_ringBufferIndex / sizeElements);
    }

    private:
    /**
     * @brief Loads the current active part from the ring buffer and cycles the internal index to the next active part 
     * 
     */
    void loadNext() {
        // TODO(bwintermann): Since this gets called automatically when syncing, maybe make private?
        // TODO(bwintermann): Iterators?
        for (unsigned int i = 0; i < sizeElements; i++) {
            map[i] = ringBuffer[(_ringBufferIndex + i)  % ringBufferSizeElements];
        }
        _ringBufferIndex = (_ringBufferIndex + sizeElements) % ringBufferSizeElements;
    }

    /**
     * @brief Load a specific partIndex to the bo map. Afterwards the internal index points to the next part after partIndex 
     * 
     * @param partIndex 
     */
    void load(unsigned int partIndex) {
        _ringBufferIndex = partIndex * sizeElements;
        loadNext();
    }

    public:
    /**
     * @brief Set an active ring buffer part to values given by an array 
     * 
     * @param partIndex The part to set to. Must be smaller than size() 
     * @param makeCurrent Whether or not to update the internal counter (e.g. for usage with loadNext()) to the newly written part
     * @param arr The array to copy the data from
     * @param arrSize The size of the array to copy data from
     */
    void set(const unsigned int partIndex, const bool makeCurrent = true, const T& arr, const unsigned int arrSize) {
        // TODO(bwintermann): Optimize this method
        // TODO(bwintermann): OPERATOR OVERLOADING
        if (partIndex >= ringBufferActiveParts) {
            throw std::length_error("Trying to assign a DeviceBuffer at index " + std::to_string(partIndex) + " but the max index is " + std::to_string(ringBufferActiveParts));
        }
        if (arrSize != sizeElements) {
            throw std::length_error("Mismatch in sizes: Trying to assign a ring buffer active part with an array of size " + std::to_string(arrSize) + " but a ring buffer active part has " + std::to_string(sizeElements) + " elements!");
        }
        for (unsigned int i = 0; i < arrSize; i++) {
            ringBuffer[(partIndex * sizeElements + i) % ringBufferSizeElements] = arr[i];
        }

        if (makeCurrent) {
            _ringBufferIndex = partIndex * sizeElements;
        }
    }

    /**
     * @brief Writes the specified ring buffer part into the target array. The array must be large enough to hold the number of elements in one ring buffer part (can be found out with size(SIZE_SPECIFIER::ELEMENTS)) 
     * 
     * @param partIndex 
     * @param target 
     */
    void get(const unsigned int partIndex, T& target) {
        // TODO(bwintermann): Make this method more efficient
        // TODO(bwintermann): OPERATOR OVERLOADING
        for (unsigned int i = 0; i < sizeElements; i++) {
            target[i] = ringBuffer[(partIndex * sizeElements + i) % ringBufferSizeElements];
        }
    }

    /**
     * @brief Load the specified partIndex of the buffer into the memory map and sync the map to the FPGA. The internal partIndex will point to the next part afterwards. 
     * 
     * @param partIndex The part index to load 
     * @return unsigned int 
     */
    unsigned int sync(unsigned int partIndex) {
        if (bufferIOMode == IO::INPUT || bufferIOMode == IO::OUTPUT) {
            return sync(bufferIOMode, partIndex, true);
        }
        throw std::runtime_error("Invalid IO Mode when syncing!")
    }

    /**
     * @brief Load the next part that is specified by the internal index into the memory map and sync the memory map to the FPGA. The internal partIndex will point to the next part afterwards. 
     * 
     * @return unsigned int 
     */
    unsigned int sync() {
        if (bufferIOMode == IO::INPUT || bufferIOMode == IO::OUTPUT) {
            return sync(bufferIOMode, 0, false);
        }
        throw std::runtime_error("Invalid IO Mode when syncing!")
    }

    
    private:
    /**
     * @brief Private method to sync memory map to device. Do not call directly, call sync() or sync(partIndex) instead.
     * 
     * @param direction 
     * @param partIndex
     * @param loadCustomPart 
     * @return unsigned 
     */
    unsigned int _sync(IO direction, unsigned int partIndex, bool loadCustomPart) {
        if (loadCustomPart) {
            load(partIndex);
        } else {
            loadNext();
        }
        if (direction == IO::INPUT) {
            _bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        } else if (direction == IO::OUTPUT) {
            _bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        } else {
            throw std::runtime_error("Specify either input or output as IO mode when calling sync manually (INOUT or UNSPECIFIED are invalid values)!");
        }
        return getCurrentIndex();
    }

    public:
    /**
     * @brief Set the whole buffer to 0's 
     * 
     */
    void clear() {
        // TODO(bwintermann): Reimplement with iterators
        for (unsigned int i = 0; i < ringBuffer.size(); i++) {
            ringBuffer[i] = 0;
        }
    }

};