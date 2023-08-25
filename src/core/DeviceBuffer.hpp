#include <boost/circular_buffer.hpp>
#include <magic_enum.hpp>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Types.h"
#include "../utils/Logger.h"

#include "xrt.h"
#include "xrt/xrt_bo.h"


/**
 * @brief Proxy class used to enable operator[] overloading for the DeviceBuffer, since it internally works on a more fine grained ring buffer, which was to be managed when assigning and reading 
 * 
 * @tparam T The datatype found in the ring buffer of the DeviceBuffer  
 */
template<typename T>
class RingBufferAssignmentProxy {
     private:
    boost::circular_buffer<T>& ringBuffer;
    unsigned int partIndex;
    unsigned int elementIndex;
    unsigned int elementsPerPart;

     public:
    RingBufferAssignmentProxy(boost::circular_buffer<T>& pRingBuffer, unsigned int pPartIndex, unsigned int pElementsPerPart) : ringBuffer(pRingBuffer), partIndex(pPartIndex), elementsPerPart(pElementsPerPart) {
        elementIndex = partIndex * elementsPerPart;
    }

    /**
     * @brief Handle assignment by setting the ring buffer values to the values from the passed vector. If the vector is larger than the part of the buffer that was indexed, the overflowing rest of the buffer will be ignored 
     * 
     * @param setValues 
     * @return RingBufferAssignmentProxy& 
     */
    RingBufferAssignmentProxy& operator=(const std::vector<T>& setValues) {
        // Cuts vector off if too large
        for (unsigned int i = 0; i < elementsPerPart; i++) {
            ringBuffer[(elementIndex + i) % ringBuffer.size()] = setValues[i];
        }
        return *this;
    }

    /**
     * @brief Retrieve a part from the device buffer and unpack it into a vector  
     * 
     * @return std::vector<T> 
     */
    std::vector<T> unpackToVector() {
        std::vector<T> temp;
        for (unsigned int i = 0; i < elementsPerPart; i++) {
            temp.push_back(ringBuffer[(partIndex * elementsPerPart + i) % ringBuffer.size()]);
        }
        return temp;
    }

    /**
     * @brief Retrieve a part from the device buffer and unpack it into an array 
     * 
     * @tparam S The size of the array to be constructed. If the passed size is too small, a length error is thrown.
     * @return std::array<T,S> 
     */
    template<std::size_t S>
    std::array<T,S> unpackToArray() {
        if (S < elementsPerPart) {
            throw std::length_error("Tried to unpack " + std::to_string(elementsPerPart) + " elements into an array of size " + std::to_string(S));
        }
        std::array<T,S> temp;
        for (unsigned int i = 0; i < elementsPerPart; i++) {
            temp[i] = ringBuffer[(partIndex * elementsPerPart + i) % ringBuffer.size()];
        }
        return temp;
    }

};


/**
 * @brief
 *
 * @tparam T The smallest unit of data that a device manager has to manage
 * @tparam F The FINN Datatype that this buffer receives. It is represented by one or multiple T's
 */
template<typename T, typename F>
class DeviceBuffer {
    const std::string name;
    /**
     * @brief Number of numbers in a given input. For a Tensor with shape (1, 3, 4) this would be 1*3*4 
     * 
     */
    unsigned int sizeNumbers;
    
    /**
     * @brief Number of elements that make up the data. If the tensor uses an INT16, this might be represented by two int8's, so that sizeElements = 2 * sizeNumbers 
     * 
     */
    unsigned int sizeElements;
    
    /**
     * @brief Number of bytes that make up all elements. If the tensor is of type INT32, represented here by int16_t's, then sizeElements = 2 * sizeNumbers and sizeBytes = 2 * sizeElemenets = 4 * sizeNumbers 
     * 
     */
    size_bytes_t sizeBytes;  // sizeof(T) * elements

    const IO bufferIOMode;
    xrt::bo internalBo;
    T* map;
    const shape_t bufferShape;

    unsigned int ringBufferElementSize;  //! Size of ALL elements, not per active part
    unsigned int ringBufferActiveParts;
    unsigned int ringBufferElementIndex = 0;
    boost::circular_buffer<T> ringBuffer;

    logger_type& logger = Logger::getLogger();

     public:
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    DeviceBuffer(const std::string& pName, xrt::device& device, const shape_t pShape, unsigned int ringBufferSizeFactor, IO pBufferIOMode)
        : name(pName),
          sizeNumbers(static_cast<unsigned int>(std::accumulate(pShape.begin(), pShape.end(), 1, std::multiplies<>()))),
          sizeElements(sizeNumbers * F().template requiredElements<T>()),
          sizeBytes(sizeof(T) * sizeElements),
          bufferIOMode(pBufferIOMode),
          internalBo(xrt::bo(device, sizeBytes, 0)), /*TODO(bwintermann): Check if xrt::bo really takes bytes and not elements!*/
          map(internalBo.map<T*>()),
          bufferShape(pShape),
          ringBufferElementSize(sizeElements * ringBufferSizeFactor),
          ringBufferActiveParts(ringBufferSizeFactor),
          ringBuffer(boost::circular_buffer<T>(ringBufferElementSize)) {
        assert(ringBuffer.size() == ringBuffer.capacity());
        assert(ringBufferElementSize % sizeElements == 0);
        std::string tempShapeStr(bufferShape.begin(), bufferShape.end());
        FINN_LOG(logger, loglevel::info) << "Instantiated "<< magic_enum::enum_name(bufferIOMode) << " DeviceBuffer " << name << " (Shape: " << tempShapeStr << ", Numbers: " << sizeNumbers << ", Elements: " << sizeElements << ", Bytes: " << sizeBytes << " and Buffer Elements: " << ringBufferElementSize << ")\n"; 
    }

    bool isInputBuffer() const { return bufferIOMode == IO::INPUT; };
    bool isOutputBuffer() const { return bufferIOMode == IO::OUTPUT; };
    bool isInOutBuffer() const { return bufferIOMode == IO::INOUT; };

    /**
     * @brief Return the size of the buffer / map. Specifying BYTES returns the number of bytes, NUMBERS how many numbers the buffer has to store for one sample, ELEMENTS how many T's are required to store all NUMBERS and SAMPLE 1.
     *
     * @param ss The type of size
     * @return unsigned int
     */
    unsigned int size(SIZE_SPECIFIER ss) const {
        if (ss == SIZE_SPECIFIER::BYTES) {
            return static_cast<unsigned int>(sizeBytes);
        } else if (ss == SIZE_SPECIFIER::ELEMENTS) {
            return sizeElements;
        } else if (ss == SIZE_SPECIFIER::NUMBERS) {
            return sizeNumbers;
        } else if (ss == SIZE_SPECIFIER::SAMPLES) {
            return 1;
        } else {
            FinnUtils::logAndError<std::runtime_error>("Unknown size specifier!");
        }
    }

    /**
     * @brief In this overloaded default size() function, the number of active parts of the ring buffer is returned.
     * This method should be used when the DeviceBuffer is treated like an array
     *
     * @return unsigned int
     */
    unsigned int size() const { return ringBufferActiveParts; }

    /**
     * @brief Get the index of the current active buffer part! This does NOT return the actual ring buffer element-wise index!
     * If the elementwise index is between to parts, the index of the lower is returned. (E.g. if Part0 goes from 0-100 and the elementwise index is 50, then 0 is returned as the part index) 
     *
     * @return unsigned int
     */
    unsigned int getCurrentIndex() const { return ringBufferElementIndex / sizeElements; }

     private:
    /**
     * @brief Loads the current active part from the ring buffer and cycles the internal index to the next active part
     *
     */
    void loadNext() {
        // TODO(bwintermann): Since this gets called automatically when syncing, maybe make private?
        // TODO(bwintermann): Iterators?
        for (unsigned int i = 0; i < sizeElements; i++) {
            map[i] = ringBuffer[(ringBufferElementIndex + i) % ringBufferElementSize];
        }
        ringBufferElementIndex = (ringBufferElementIndex + sizeElements) % ringBufferElementSize;
    }

    /**
     * @brief Load a specific partIndex to the bo map. Afterwards the internal index points to the next part after partIndex
     *
     * @param partIndex
     */
    void load(unsigned int partIndex) {
        ringBufferElementIndex = partIndex * sizeElements;
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
    void set(const unsigned int partIndex, const T& arr, const unsigned int arrSize, bool makeCurrent = true) {
        // TODO(bwintermann): Optimize this method
        // TODO(bwintermann): OPERATOR OVERLOADING
        if (partIndex >= ringBufferActiveParts) {
            // clang-tidy warnings in this line are false positive: https://github.com/llvm/llvm-project/issues/57959
            //  NOLINTNEXTLINE
            throw std::length_error("Trying to assign a DeviceBuffer at index " + std::to_string(partIndex) + " but the max index is " + std::to_string(ringBufferActiveParts));
        }
        if (arrSize != sizeElements) {
            // clang-tidy warnings in this line are false positive: https://github.com/llvm/llvm-project/issues/57959
            // NOLINTNEXTLINE
            throw std::length_error("Mismatch in sizes: Trying to assign a ring buffer active part with an array of size " + std::to_string(arrSize) + " but a ring buffer active part has " + std::to_string(sizeElements) + " elements!");
        }
        for (unsigned int i = 0; i < arrSize; i++) {
            ringBuffer[(partIndex * sizeElements + i) % ringBufferElementSize] = arr[i];
        }

        if (makeCurrent) {
            ringBufferElementIndex = partIndex * sizeElements;
        }
    }

    /**
     * @brief Writes the specified ring buffer part into the target array. The array must be large enough to hold the number of elements in one ring buffer part (can be found out with size(SIZE_SPECIFIER::ELEMENTS))
     *
     * @param partIndex
     * @param target
     */
    void get(const unsigned int partIndex, T& target) const {
        // TODO(bwintermann): Make this method more efficient
        // TODO(bwintermann): OPERATOR OVERLOADING
        for (unsigned int i = 0; i < sizeElements; i++) {
            target[i] = ringBuffer[(partIndex * sizeElements + i) % ringBufferElementSize];
        }
    }

    /**
     * @brief Returns a RingBufferAssignmentProxy, which can either be written to using the = operator, or read from, by using one of several unpack-methods to convert the data into the correct types
     *
     * @param index
     * @return RingBufferAssignmentProxy<T>
     */
    RingBufferAssignmentProxy<T> operator[](unsigned int index) { return RingBufferAssignmentProxy<T>(ringBuffer, index, sizeElements); }

    /**
     * @brief Load the specified partIndex of the buffer into the memory map and sync the map to the FPGA. The internal partIndex will point to the next part afterwards.
     *
     * @param partIndex The part index to load
     * @return unsigned int
     */
    unsigned int sync(unsigned int partIndex) {
        FINN_LOG_DEBUG(logger, loglevel::debug) << "Syncing buffer " << name << " with mode " << magic_enum::enum_name(bufferIOMode);
        if (bufferIOMode == IO::INPUT || bufferIOMode == IO::OUTPUT) {
            return syncHelper(bufferIOMode, partIndex, true);
        }
        FinnUtils::logAndError<std::runtime_error>("Invalid IO Mode when syncing!");
    }

    /**
     * @brief Load the next part that is specified by the internal index into the memory map and sync the memory map to the FPGA. The internal partIndex will point to the next part afterwards.
     *
     * @return unsigned int
     */
    unsigned int sync() {
        FINN_LOG_DEBUG(logger, loglevel::debug) << "Syncing buffer " << name << " with mode " << magic_enum::enum_name(bufferIOMode);
        if (bufferIOMode == IO::INPUT || bufferIOMode == IO::OUTPUT) {
            return syncHelper(bufferIOMode, 0, false);
        }
        FinnUtils::logAndError<std::runtime_error>("Invalid IO Mode when syncing!");
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
    unsigned int syncHelper(IO direction, unsigned int partIndex, bool loadCustomPart) {
        if (loadCustomPart) {
            load(partIndex);
        } else {
            loadNext();
        }
        if (direction == IO::INPUT) {
            internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        } else if (direction == IO::OUTPUT) {
            internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        } else {
            FinnUtils::logAndError<std::runtime_error>("Specify either input or output as IO mode when calling sync manually (INOUT or UNSPECIFIED are invalid values)!");
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