/**
 * @file ringBuffer.hpp
 * This file contains code for a RingBuffer class, specifically tailored for use in the FINN driver.
 *
 */


template<typename T>
class RingBuffer {
    std::unique_ptr<T> data;
    unsigned int currentIndex;
    unsigned int size;
    unsigned int targetSize;

     public:
    /**
     * @brief Construct a new Ring Buffer object.
     *
     * @param pSize Size of this buffer. Should be a whole multiple of targetSize
     * @param pTargetSize Size of the memory-map this is later mapped to
     */
    RingBuffer(const unsigned int pSize, const unsigned int pTargetSize) : size(pSize), targetSize(pTargetSize) {
        data = std::make_unique<T[]>(pSize);
        currentIndex = 0;
    }

    RingBuffer(const std::vector<T>& elements, const unsigned int pTargetSize) : targetSize(pTargetSize) {
        size = elements.size();
        data = std::make_unique<T[]>(size);
        for (unsigned int i = 0; i < size; i++) {
            data[i] = elements[i];
        }
        currentIndex = 0;
    }

    RingBuffer(const T* arr, const unsigned int pSize, const unsigned int pTargetSize) : targetSize(pTargetSize) {
        data = std::make_unique<T[]>(pSize);
        for (unsigned int i = 0; i < pSize; i++) {
            data[i] = arr[i];
        }
        currentIndex = 0;
    }

    /**
     * @brief Append a single object to the buffer. If the current index overflows, it is wrapped around.
     *
     * @param element
     */
    void appendElement(const T element) {
        data[currentIndex] = element;
        if (currentIndex == size - 1) {
            currentIndex = 0;
        }
    }

    /**
     * @brief Append the elements from the given array to the ring buffer
     *
     * @param arr
     * @param arrSize
     */
    void appendElements(const T* arr, unsigned int arrSize) {
        for (unsigned int i = 0; i < arrSize; i++) {
            appendElement(arr[i]);
        }
    }

    /**
     * @brief Append the elements from the given vector to the ring buffer
     *
     * @param elements
     */
    void appendElements(const std::vector<T>& elements) {
        for (auto elem : elements) {
            appendElement(elem);
        }
    }

    /**
     * @brief Get the Inverse Index of the current index (opposite index on the ring)
     *
     * @return unsigned int
     */
    unsigned int getInverseIndex() { return (currentIndex + static_cast<unsigned int>(size / 2)) % size; }

    unsigned int getCurrentIndex() { return currentIndex; }

    void resetIndex() { currentIndex = 0; }

    unsigned int getSize() { return size; }

    /**
     * @brief Cycle the index to the start of the next part of the ring buffer
     *
     */
    void cycleActivePart() { currentIndex = (currentIndex + targetSize) % size; }

    /**
     * @brief In most cases this buffer is used on a buffer-map for a xrt::bo buffer, having a multiple of it's size. This copies the "active part" (next targetSize elements) to a target array.
     * This is unsafe because the array size is not safely given necessarily.
     *
     * @param target The target array. It's size must be RingBuffer.targetSize!!
     * @param updateIndex Whether to move the currentIndex forward to the next active part
     */
    void copyActivePartToArray(T* target, bool updateIndex) {
        for (unsigned int i = 0; i < targetSize; i++) {
            target[i] = data[(currentIndex + i) % size];
        }
        if (updateIndex) {
            cycleActivePart();
        }
    }

    /**
     * @brief In most cases this buffer is used on a buffer-map for a xrt::bo buffer, having a multiple of it's size. This copies the array into the "active part" (next targetSize elements).
     * This is unsafe because the array size is not safely given necessarily.
     *
     * @param source The source array. It's size must be RingBuffer.targetSize!!
     * @param updateIndex Whether to move the currentIndex forward to the next active part
     */
    void copyArrayToActivePart(T* source, bool updateIndex) {
        for (unsigned int i = 0; i < targetSize; i++) {
            data[(currentIndex + i) % size] = target[i];
        }
        if (updateIndex) {
            cycleActivePart();
        }
    }

    /**
     * @brief Fill the buffer with random integer values ranging from min to max
     * @deprecated This has to perform possibly weird conversions from int -> T. To be replaced by a more general and powerful method
     *
     * @param min
     * @param max
     */
    void fillRandomInt(int min, int max) {
        for (unsigned int i = 0; i < size; i++) {
            data[i] = std::experimental::randint(min, max);
        }
    }
};