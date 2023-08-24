/**
 * @file memoryMap.h
 * This file contains code for the MemoryMap struct, which is a wrapper around a C-style array that serves as a input/output map to an xrt::bo object. It provides the array's size in byte and elements, keeps track of it's dimensions
 * (shapes) and shape-type. It also contains a RingBuffer object that can be used to efficiently put new data into the xrt::bo to sync to a device. A MemoryMap is usually owned by a DeviceHandler.
 */

#ifndef MEMORY_MAP
#define MEMORY_MAP

#include "ringBuffer.hpp"
#include "types.h"

/**
 * @brief A struct to wrap the memory map that a xrt::bo object writes/reads to/from. It also contains information about the buffers tensor shape, it's size in bytes and its shape type, as well as convenience functions for interaction
 *
 * @tparam T The datatype of the memory map
 */
template<typename T>
struct MemoryMap {
    /**
     * @brief The buffers/maps name as given by the FINN export
     *
     */
    const std::string name;

    /**
     * @brief Stores the Buffer of memory used for communication
     *
     */
    T* map = nullptr;
    /**
     * @brief Size of the mapped buffer (in bytes (?)) To get the number of elements use getElementCount()
     *
     */
    std::size_t size = 0;
    /**
     * @brief Dimensions of the buffer (in elements)
     *
     */
    std::vector<unsigned int> dims = {};
    /**
     * @brief Layout of the buffer
     *
     */
    SHAPE_TYPE shapeType = SHAPE_TYPE::INVALID;

    /**
     * @brief RingBuffer used to quickly supply new values to the memory map
     *
     */
    RingBuffer<T> ringBuffer;

    /**
     * @brief Get the number of elements contained in the memory map
     *
     * @return unsigned int
     */
    unsigned int getElementCount() const { return static_cast<unsigned int>(size / sizeof(T)); }

    /**
     * @brief Load a new set of data from the internal ring buffer.
     *
     * @param cycle Whether or not the ring buffer internal index should be updated ((recommended true)).
     */
    void loadFromRingBuffer(bool cycle) {
        if (ringBuffer.targetSize != getElementCount()) {
            throw std::length_error("Ring buffer of memory map " + name + " has target size " + std::to_string(ringBuffer.targetSize) + " but the memory map actual size is " + std::to_string(getElementCount()));
        }
        ringBuffer.copyActivePartToArray(map, cycle);
    }

    /**
     * @brief Write a single element into the given map index
     *
     * @param elem The element itself
     * @param index The index of the map where to place the element
     * @return BUFFER_OP_RESULT
     */
    BUFFER_OP_RESULT writeSingleElement(const T& elem, const unsigned int index) {
        if (index >= getElementCount()) {  // Check against negative values is unnecessary, because type is unsigned.
            return BUFFER_OP_RESULT::OVER_BOUNDS_WRITE;
        }
        map[index] = elem;
        return BUFFER_OP_RESULT::SUCCESS;
    }

    /**
     * @brief Writes the given elements into the data map, which is mapped to the XRT buffer object. Returns whether or not the operation was successful.
     *
     * @param elements The container with the elements to write.
     * @param startIndex The index from which the map should be accessed, so if startIndex=2, the map fields 2,3,4... are being written
     * @return BUFFER_OP_RESULT
     */
    BUFFER_OP_RESULT writeElements(const std::vector<T>& elements, const unsigned int startIndex) {
        if (startIndex + elements.size() > getElementCount()) {
            return BUFFER_OP_RESULT::OVER_BOUNDS_WRITE;
        }
        for (unsigned int i = 0; i < elements.size(); ++i) {
            map[i + startIndex] = elements[i];
        }
        return BUFFER_OP_RESULT::SUCCESS;
    }

    /**
     * @brief Read elements from the XRT buffer object mapped pointer. Returns whether or not the operation was successful.
     *
     * @param readBuffer The buffer to read into. Needs to be large enough to contain elementCount elements.
     * @param startIndex The offset to read the map from
     * @param elementCount The number of elements to read
     * @return BUFFER_OP_RESULT
     */
    BUFFER_OP_RESULT readElements(std::vector<T>& readBuffer, const unsigned int startIndex, const unsigned int elementCount) const {
        if (startIndex + elementCount > getElementCount()) {
            return BUFFER_OP_RESULT::OVER_BOUNDS_READ;
        }
        for (unsigned int i = 0; i < elementCount; ++i) {
            readBuffer[i] = map[startIndex + i];
        }
        return BUFFER_OP_RESULT::SUCCESS;
    }
};


#endif