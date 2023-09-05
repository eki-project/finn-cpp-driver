#include <span>
#include <mutex>
#include <boost/circular_buffer.hpp>

#include "FinnDatatypes.hpp"
#include "Types.h"
#include "Logger.h"

/**
 * @brief Wrapper class for boost::circular_buffer, which handles abstraction.
 * This wrapper keeps track of a "head" pointer, as well as which parts of the buffer are valid data or not. A part is made up from several values of type T. 
 * 
 * @tparam T 
 */
template<typename T>
class RingBuffer {
    boost::circular_buffer<T> buffer;
    std::vector<bool> validParts;
    std::vector<std::unique_ptr<std::mutex>> partMutexes;
    const size_t parts;
    const size_t elementsPerPart;
    index_t headPart = 0;
    logger_type& logger;

    /**
     * @brief Construct a new Ring Buffer object. It's size in terms of values of type T is given by pElementsPerPart * pParts. By default all parts are invalid data to start with.
     * 
     * @param pParts 
     * @param pElementsPerPart 
     */
    public:
    RingBuffer(const size_t pParts, const size_t pElementsPerPart) : 
        buffer(boost::circular_buffer<T>(pElementsPerPart * pParts)),
        validParts(std::vector<bool>(pParts)),
        parts(pParts),
        elementsPerPart(pElementsPerPart),
        logger(Logger::getLogger())
    {
        for (size_t i = 0; i < parts; i++) {
            partMutexes.emplace_back(std::make_unique<std::mutex>());
        }
        std::fill(validParts.begin(), validParts.end(), false);
        buffer.resize(pElementsPerPart * pParts);
        FINN_LOG(logger, loglevel::info) << "Initialized RingBuffer (PARTS: " << parts << ", ELEMENTS: " << buffer.size() << ", ELEMENTS PER PART: " << elementsPerPart << ", BYTES: " << buffer.size() / sizeof(T) << ")\n";
    }

    private:
    /**
     * @brief Return the element-wise index of the ring buffer, considering the part partIndex with it's element offset. (Useful for loops) 
     * 
     * @param partIndex 
     * @param offset 
     * @return index_t 
     */
    index_t elementIndex(index_t partIndex, index_t offset) const {
        return (partIndex * elementsPerPart + offset) % buffer.size();
    }

    /**
     * @brief Set the validity of a part given by it's index
     * @attention Must be private to guarantee thread safety. The corresponding mutex is managed by the store/read methods
     * @param partIndex 
     * @param validity 
     */
    void setPartValidity(index_t partIndex, bool validity) {
        if (partIndex > parts) {
            FinnUtils::logAndError<std::length_error>("Tried setting validity for an index that is too large.");
        }
        validParts[partIndex] = validity;
    }

    public:
    /**
     * @brief Thread safe version of the internal setValidity method.
     * @attention Dev: This should NOT be used in a store/read method which also already locks the part mutex!
     * 
     * @param partIndex 
     * @param validity 
     */
    void setPartValidityMutexed(index_t partIndex, bool validity) {
        if (partIndex > parts) {
            FinnUtils::logAndError<std::length_error>("Tried setting validity for an index that is too large.");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        validParts[partIndex] = validity;
    }

    /**
     * 
     * @brief Return the RingBuffer's size, either in elements of T, in bytes or in parts 
     * 
     * @param ss 
     * @return size_t 
     */
    size_t size(SIZE_SPECIFIER ss) const {
        if (ss == SIZE_SPECIFIER::ELEMENTS) {
            return buffer.size();
        } else if (ss == SIZE_SPECIFIER::BYTES) {
            return buffer.size() * sizeof(T);
        } else if (ss == SIZE_SPECIFIER::PARTS) {
            return parts;
        } else if (ss == SIZE_SPECIFIER::ELEMENTS_PER_PART) {
            return elementsPerPart;
        } else {
            FinnUtils::logAndError<std::runtime_error>("Unknown size specifier!");
            return 0;
        }
    }

    /**
     * @brief Count the number of valid parts in the buffer. 
     * 
     * @return unsigned int 
     */
    unsigned int countValidParts() const {
        return static_cast<unsigned int>(std::count_if(validParts.begin(), validParts.end(), [](bool x){return x;}));
    }

    /**
     * @brief Get the opposite index from the current head index.
     * When the number of parts is odd, e.g. 13 and the pointer at 3, this would return 10, because the ceil'd half of parts is added onto the head index. 
     * @return index_t 
     */
    index_t getHeadOpposite() const {
        return (
            headPart + static_cast<index_t>(
                FinnUtils::ceil(
                    static_cast<float>(parts)/2.0F
                )
            )
        ) % parts;
    }

    /**
     * @brief Get the head part index
     * 
     * @return * index_t 
     */
    index_t getHeadIndex() const {
        return headPart;
    }


    /**
     * @brief Increment the head pointer by one part. Loops around when the highest index is met. 
     * 
     */
    void cycleHeadPart() {
        headPart = (headPart + 1) % parts;
    }

    /**
     * @brief Set the head pointer to a given index
     *       
     * @param partIndex 
     */
    void setHeadPart(index_t partIndex) {
        if (partIndex >= parts) {
            FinnUtils::logAndError<std::length_error>("Couldnt set head index manually, value too high!");
        }
        headPart = partIndex;
    }

    /**
     * @brief Check the validity of a given partIndex 
     * 
     * @param partIndex 
     * @return true 
     * @return false 
     */
    bool isPartValid(index_t partIndex) const {
        return partIndex < parts && validParts[partIndex];
    }

    /**
     * @brief Check the validity of the head pointer part 
     * 
     * @return true 
     * @return false 
     */
    bool isPartValid() const {
        return validParts[headPart];
    }

    /**
     * @brief Returns true, if all parts of the buffer are marked as valid 
     * 
     * @return true 
     * @return false 
     */
    bool isFull() const {
        return countValidParts() == parts;
    }

    /**
     * @name Storage / Input methods
     * These methods are used to insert data into the buffer in various ways. The store-methods increment the head pointer, while setting the part that was just written to valid.
     * The setPart-methods operate on a given index instead of the head index, they do not change the head pointer, and you can choose whether the validity of the newly inserted part should be set to true or false. 
     * 
     */
    ///@{
    /**
     * @brief Insert a vector at the head pointer part, set the part valid, increase the pointer.
     * 
     * @param vec 
     */
    void store(const std::vector<T>& vec) {
        if (vec.size() != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer!");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[headPart]);
        for (size_t i = 0; i < vec.size(); i++) {
            buffer[elementIndex(headPart, i)] = vec[i];
        }
        setPartValidity(headPart, true);
        cycleHeadPart();
    }

    /**
     * @brief Insert an array at the head pointer part, set the part valid, increase the pointer.
     * 
     * @tparam size
     * @param arr 
     */
    template<size_t sizetem>
    void store(std::array<T,sizetem> arr) {
        if (arr.size() != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when storing array in Ring Buffer!");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[headPart]);
        for (index_t i = 0; i < arr.size(); i++) {
            buffer[elementIndex(headPart, i)] = arr[i];
        }
        setPartValidity(headPart, true);
        cycleHeadPart();
    }

    /**
     * @brief Set the given partIndex to the passed vector values, do NOT increment the head pointer, and set the validity to the passed value
     * 
     * @param vec 
     * @param partIndex 
     * @param validity 
     */
    void setPart(const std::vector<T>& vec, index_t partIndex, bool validity) {
        if (vec.size() != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer!");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        for (size_t i = 0; i < vec.size(); i++) {
            buffer[elementIndex(partIndex, i)] = vec[i];
        }
        setPartValidity(partIndex, validity);
    }

    /**
     * @brief Set the given partIndex to the passed array values, do NOT increment the head pointer, and set the validity to the passed value
     * 
     * @tparam size
     * @param arr 
     * @param partIndex 
     * @param validity 
     */
    template<size_t size>
    void setPart(const std::array<T,size>& arr, index_t partIndex, bool validity) {
        if (arr.size() != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer!");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        for (size_t i = 0; i < arr.size(); i++) {
            buffer[elementIndex(partIndex, i)] = arr[i];
        }
        setPartValidity(partIndex, validity);
    }
    ///@}



    /**
     * @name Read / Output methods
     * Just as with the store- and setPart methods, read and getPart read a part from the buffer. In similar fashion, read() reads from the head pointer, increments the head pointer and invalidates the just read part. The
     * getPart methods read from the given partIndex, do NOT increase the head pointer and set the validity of the just read part as wished. 
     */
    ///@{

    /**
     * @brief Read from the head pointer and return as a vector. Increments head pointer and invalidates read data. 
     * 
     * @return std::vector<T> 
     */
    std::vector<T> read() {
        std::lock_guard<std::mutex> guard(*partMutexes[headPart]);
        std::vector<T> temp;
        for (size_t i = 0; i < elementsPerPart; i++) {
            temp.push_back(buffer[elementIndex(headPart, i)]);
        }
        setPartValidity(headPart, false);
        cycleHeadPart();
        return temp;
    }

    /**
     * @brief Read from head pointer returns an std::array. Increments head pointer and invalidates read data. 
     * 
     * @tparam S 
     * @return std::array<T,S> 
     */
    template<size_t S>
    std::array<T,S> read() {
        std::lock_guard<std::mutex> guard(*partMutexes[headPart]);
        std::array<T,S> arr;
        for (size_t i = 0; i < elementsPerPart; i++) {
            arr[i] = buffer[elementIndex(headPart, i)];
        }
        setPartValidity(headPart, false);
        cycleHeadPart();
        return arr;
    }

    /**
     * @brief Read from head pointer into the referenes array. Increments head pointer and invalidates read data. 
     * 
     * @param targetArr 
     * @param arrSize 
     */
    void read(T* targetArr, const size_t& arrSize) {
        if (arrSize != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatching when trying to read ring buffer data into an array!");
        }

        std::lock_guard<std::mutex> guard(*partMutexes[headPart]);
        for (size_t i = 0; i < elementsPerPart; i++) {
            targetArr[i] = buffer[elementIndex(headPart, i)];
        }
        setPartValidity(headPart, false);
        cycleHeadPart();
    }

    /**
     * @brief Read from the given partIndex, do NOT change the head pointer and set validity as prompted. Returns data as a vector
     * 
     * @param partIndex 
     * @param validity 
     * @return std::vector<T> 
     */
    std::vector<T> getPart(const index_t partIndex, const bool validity) {
        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        std::vector<T> temp;
        for (size_t i = 0; i < elementsPerPart; i++) {
            temp.push_back(buffer[elementIndex(partIndex, i)]);
        }
        setPartValidity(partIndex, validity);
        return temp;
    }

    /**
     * @brief Read from the given partIndex, do NOT change the head pointer and set validity as prompted.
     * 
     * @tparam S 
     * @param partIndex 
     * @param validity 
     * @return std::array<T,S> 
     */
    template<size_t S>
    std::array<T,S> getPart(index_t partIndex, bool validity) {
        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        std::array<T,S> arr;
        for (size_t i = 0; i < elementsPerPart; i++) {
            arr[i] = buffer[elementIndex(partIndex, i)];
        }
        setPartValidity(partIndex, validity);
        return arr;
    }

    /**
     * @brief Read from the given partIndex, do NOT change the head pointer and set validity as prompted. Writes data into referenced array
     * 
     * @param arr 
     * @param arrSize 
     * @param partIndex 
     * @param validity 
     */
    void getPart(T* arr, const size_t arrSize, index_t partIndex, bool validity) {
        if (arrSize != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatching when trying to read ring buffer data into an array!");
        }
        
        std::lock_guard<std::mutex> guard(*partMutexes[partIndex]);
        for (size_t i = 0; i < elementsPerPart; i++) {
            arr[i] = buffer[elementIndex(partIndex, i)];
        }
        setPartValidity(partIndex, validity);
    }
    ///@}
};