#ifndef DRIVER_HPP
#define DRIVER_HPP

enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2 };

enum class BUFFER_OP_RESULT { SUCCESS = 0, FAILURE = -1, OVER_BOUNDS_WRITE = -2, OVER_BOUNDS_READ = -3 };

enum class TRANSFER_MODE {
    MEMORY_BUFFERED = 0, STREAMED = 1
};

/**
 * @brief A struct to wrap the memory map that a xrt::bo object writes/reads to/from. It also contains information about the buffers tensor shape, it's size in bytes and its shape type, as well as convenience functions for interaction
 *
 * @tparam T The datatype of the memory map
 */
template<typename T>
struct MemoryMap {
    T* map;
    std::size_t size;
    std::initializer_list<unsigned int> dims;
    SHAPE_TYPE shapeType;

    /**
     * @brief Get the number of elements contained in the memory map
     *
     * @return unsigned int
     */
    const unsigned int getElementCount() const { return static_cast<unsigned int>(size / sizeof(T)); }

    /**
     * @brief Write a single element into the given map index
     *
     * @param elem The element itself
     * @param index The index of the map where to place the element
     * @return BUFFER_OP_RESULT
     */
    BUFFER_OP_RESULT writeSingleElement(const T& elem, const unsigned int index) {
        if (index < 0 || index >= getElementCount()) {
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
    BUFFER_OP_RESULT readElements(std::vector<T>& readBuffer, const unsigned int startIndex, const unsigned int elementCount) {
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