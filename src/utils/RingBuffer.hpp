#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <functional>
#include <iterator>
#include <mutex>
#include <span>
#include <tuple>
#include <type_traits>

#include "FinnDatatypes.hpp"
#include "Logger.h"
#include "Types.h"

/**
 * @brief Wrapper class for boost::circular_buffer, which handles abstraction.
 * This wrapper keeps track of a "head" pointer, as well as which parts of the buffer are valid data or not. A part is made up from several values of type T.
 *
 * @tparam T
 */
template<typename T>
class RingBuffer {
    finnBoost::circular_buffer<T> buffer;
    std::vector<bool> validParts;
    const size_t parts;
    const size_t elementsPerPart;
    /**
     * @brief Points to position 1 _after_ the last written valid data entry.
     *
     */
    index_t headPart = 0;

    /**
     * @brief Points to position 1 _after_ the last read valid data entry.
     *
     */
    index_t readPart = 0;
    logger_type& logger;

    //* Mutexes
    std::mutex validPartMutex;
    std::vector<std::unique_ptr<std::mutex>> partMutexes;
    std::mutex headPartMutex;
    std::mutex readPartMutex;


    /**
     * @brief Construct a new Ring Buffer object. It's size in terms of values of type T is given by pElementsPerPart * pParts. By default all parts are invalid data to start with.
     *
     * @param pParts
     * @param pElementsPerPart
     */
     public:
    RingBuffer(const size_t pParts, const size_t pElementsPerPart)
        : buffer(finnBoost::circular_buffer<T>(pElementsPerPart * pParts)), validParts(std::vector<bool>(pParts)), parts(pParts), elementsPerPart(pElementsPerPart), logger(Logger::getLogger()) {
        for (size_t i = 0; i < parts; i++) {
            partMutexes.emplace_back(std::make_unique<std::mutex>());
        }
        std::fill(validParts.begin(), validParts.end(), false);
        buffer.resize(pElementsPerPart * pParts);
        FINN_LOG(logger, loglevel::info) << loggerPrefix() << "Initialized RingBuffer (PARTS: " << parts << ", ELEMENTS: " << buffer.size() << ", ELEMENTS PER PART: " << elementsPerPart << ", BYTES: " << buffer.size() / sizeof(T) << ")\n";
    }

    /**
     * @brief A small prefix to determine the source of the log write
     *
     * @return std::string
     */
     private:
    std::string static loggerPrefix() { return "[RingBuffer] "; }

     public:
    /**
     * @brief Move Constructor
     * @attention NOT THREAD SAFE!
     * ! This constructor is NOT THREAD SAFE!
     *
     * @param other
     */
    RingBuffer(RingBuffer&& other) noexcept
        : buffer(std::move(other.buffer)), validParts(std::move(other.validParts)), parts(other.parts), elementsPerPart(other.elementsPerPart), headPart(other.headPart), readPart(other.readPart), logger(Logger::getLogger()) {
        for (size_t i = 0; i < parts; ++i) {
            partMutexes.emplace_back(std::make_unique<std::mutex>());
        }
    }

    RingBuffer(const RingBuffer& other) = delete;
    virtual ~RingBuffer() = default;
    RingBuffer& operator=(RingBuffer&& other) = delete;
    RingBuffer& operator=(const RingBuffer& other) = delete;

     private:
    /**
     * @brief Return the element-wise index of the ring buffer, considering the part partIndex with it's element offset. (Useful for loops)
     *
     * @param partIndex
     * @param offset
     * @return index_t
     */
    index_t elementIndex(index_t partIndex, index_t offset) const { return (partIndex * elementsPerPart + offset) % buffer.size(); }


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
    unsigned int countValidParts() {
        std::lock_guard<std::mutex> guard(validPartMutex);
        return static_cast<unsigned int>(std::count_if(validParts.begin(), validParts.end(), [](bool x) { return x; }));
    }

    /**
     * @brief Returns true, if all parts of the buffer are marked as valid
     *
     * @return true
     * @return false
     */
    bool isFull() { return countValidParts() == parts; }

    /**
     * @brief Searches from the position of the head pointer to the first free (invalid data) spot and stores the data, setting the head pointer to this point+1. If no free spot is found, fase is returned
     *
     * @param data
     * @return true
     * @return false
     */
    template<typename C>
    bool store(const C data, size_t datasize) {
        if constexpr (std::is_same<C, T*>::value || std::is_same<C, std::vector<T>>::value) {
            if (datasize != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer (got " + std::to_string(datasize) + ", expected " + std::to_string(elementsPerPart) + ")!");
            }
            index_t indexP = 0;
            for (unsigned int i = 0; i < parts; ++i) {
                indexP = (headPart + i) % parts;
                if (!validParts[indexP]) {
                    //! Only now set mutex. Even if the spot just became free during the loop we'll take it, but now data has to be preserved.
                    std::lock_guard<std::mutex> guardPartMutex(*partMutexes[indexP]);
                    std::lock_guard<std::mutex> guardHeadMutex(headPartMutex);
                    for (size_t j = 0; j < datasize; ++j) {
                        buffer[elementIndex(indexP, j)] = data[j];
                    }
                    validParts[indexP] = true;
                    headPart = (indexP + 1) % parts;
                    assert((buffer[indexP * elementsPerPart + 0] == data[0]));
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Searches from the position of the head pointer to the first free (invalid data) spot and stores the data, setting the head pointer to this point+1. If no free spot is found, fase is returned
     *
     * @param data
     * @return true
     * @return false
     */
    template<typename VecUintIt>
    bool store(VecUintIt beginning, VecUintIt end) {
        static_assert(std::is_same<typename std::iterator_traits<VecUintIt>::value_type, T>::value);
        if (static_cast<unsigned long>(std::distance(beginning, end)) != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer (got " + std::to_string(std::distance(beginning, end)) + ", expected " + std::to_string(elementsPerPart) + ")!");
        }
        index_t indexP = 0;
        for (unsigned int i = 0; i < parts; ++i) {
            indexP = (headPart + i) % parts;
            if (!validParts[indexP]) {
                //! Only now set mutex. Even if the spot just became free during the loop we'll take it, but now data has to be preserved.
                std::lock_guard<std::mutex> guardPartMutex(*partMutexes[indexP]);
                std::lock_guard<std::mutex> guardHeadMutex(headPartMutex);
                for (auto [it, j] = std::tuple(beginning, 0UL); it != end; ++it, ++j) {
                    buffer[elementIndex(indexP, j)] = *it;
                }
                validParts[indexP] = true;
                headPart = (indexP + 1) % parts;
                //? Add in assert?
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Read the ring buffer and write out the first valid entry into the provided storage container. If no valid part is found, false is returned
     *
     * @param outData
     * @param datasize
     * @return true
     * @return false
     */
    bool readToVector(const std::vector<T>& outData, size_t datasize) { return read(outData, datasize); }

    /**
     * @brief Read the ring buffer and write out the first valid entry into the provided storage container. If no valid part is found, false is returned
     *
     * @param outData
     * @param datasize
     * @return true
     * @return false
     */
    bool readToArray(T* outData, size_t datasize) { return read(outData, datasize); }

     private:
    /**
     * @brief Private internal read method called by the two variants in the public namespace
     *
     * @tparam C
     * @param outData
     * @param datasize
     * @return true
     * @return false
     */
    template<typename C>
    bool read(C outData, size_t datasize) {
        if (datasize != elementsPerPart) {
            FinnUtils::logAndError<std::length_error>("Size mismatch when reading vector from Ring Buffer (got " + std::to_string(datasize) + ", expected " + std::to_string(elementsPerPart) + ")!");
        }
        index_t indexP = 0;
        for (unsigned int i = 0; i < parts; ++i) {
            indexP = (readPart + i) % parts;
            if (validParts[indexP]) {
                //! Only now set mutex. Even if the spot just became free during the loop we'll take it, but now data has to be preserved.
                std::lock_guard<std::mutex> guardPartMutex(*partMutexes[indexP]);
                std::lock_guard<std::mutex> guardHeadMutex(headPartMutex);
                for (size_t j = 0; j < elementsPerPart; ++j) {
                    outData[j] = buffer[elementIndex(indexP, j)];
                }
                validParts[indexP] = false;
                readPart = (indexP + 1) % parts;
                assert((outData[0] == buffer[indexP * elementsPerPart + 0]));
                return true;
            }
        }
        return false;
    }


#ifdef UNITTEST
     public:
    std::vector<T> testGetAsVector(index_t partIndex) {
        std::vector<T> temp;
        for (size_t i = 0; i < elementsPerPart; i++) {
            temp.push_back(buffer[elementIndex(partIndex, i)]);
        }
        return temp;
    }
    bool testGetValidity(index_t partIndex) const { return validParts[partIndex]; }
    void testSetHeadPointer(index_t i) { headPart = i; }
    void testSetReadPointer(index_t i) { readPart = i; }
    index_t testGetHeadPointer() const { return headPart; }
    index_t testGetReadPointer() const { return readPart; }
#endif
};

#endif  // RINGBUFFER_H
