#ifndef DRIVER_HPP
#define DRIVER_HPP

enum DRIVER_MODE {
    EXECUTE,
    THROUGHPUT_TEST
};

enum SHAPE_TYPE {
    NORMAL,
    FOLDED,
    PACKED
};

enum class BUFFER_OP_RESULT {
    SUCCESS = 0,
    FAILURE = -1,
    OVER_BOUNDS_WRITE = -2,
    OVER_BOUNDS_READ = -3
};

template<typename T>
__attribute__((packed)) struct MemoryMap {
    T* map;
    std::size_t size;    // In bytes
    std::initializer_list<unsigned int> dims;
    SHAPE_TYPE shapeType;
    
    /**
     * @brief Get the number of elements contained in the memory map
     * 
     * @return unsigned int 
     */
    unsigned int getElementCount() {
        return (unsigned int) (size / sizeof(T));
    }

    /**
     * @brief Writes the given elements into the data map, which is mapped to the XRT buffer object. Returns whether or not the operation was successful.
     * 
     * @param elements The container with the elements to write.  
     * @param startIndex The index from which the map should be accessed, so if startIndex=2, the map fields 2,3,4... are being written
     * @return BUFFER_OP_RESULT 
     */
    BUFFER_OP_RESULT writeElements(std::vector& elements, unsigned int& startIndex) {
        if (startIndex + elements.size() > getElementCount()) {
            return BUFFER_OP_RESULT::OVER_BOUNDS_WRITE;
        }
        for (unsigned int i = 0; i < elements.size()) {
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
    BUFFER_OP_RESULT readElements(std::vector& readBuffer, unsigned int& startIndex, unsigned int elementCount) {
        if (startIndex + elementCount > getElementCount()) {
            return BUFFER_OP_RESULT::OVER_BOUNDS_READ;
        }
        for (unsigned int i = 0; i < elementCount; i++) {
            readBuffer[i] = map[startIndex + i];
        }
        return BUFFER_OP_RESULT::SUCCESS;
    }
};


#endif