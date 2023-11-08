/**
 * @file RingBuffer.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Implements a wrapper for the circular buffer of boost
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef RINGBUFFER_HPP
#define RINGBUFFER_HPP

#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Logger.h>
#include <FINNCppDriver/utils/Types.h>

#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <execution>
#include <functional>
#include <iterator>
#include <mutex>
#include <numeric>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

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
    // cppcheck-suppress missingMemberCopy
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

// Allow public access to private methods in debug/UT mode
#ifdef NDEBUG
     private:
#else
     public:
#endif
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
     * @brief Searches from the position of the head pointer to the first free (invalid data) spot and stores the data, setting the head pointer to this point+1..
     *
     * @param data
     * @return true
     * @return false Returned if either no spot is found, or two threads tried writing to the same spot in the buffer. This results in ONE thread successfully writing the data and the other failing to do so, returning false here.
     */
    template<typename C, typename = std::enable_if_t<std::is_same<C, T*>::value || std::is_same<C, Finn::vector<T>>::value>>
    bool store(const C data, size_t datasize) {
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
                std::lock_guard<std::mutex> guardValidPartMutex(validPartMutex);

                //? Check again if part is still free to avoid collision of 2 threads. Maybe solve this by acquiring validPartMutex before searching?
                if (validParts[indexP]) {
                    return false;
                }
                for (size_t j = 0; j < datasize; ++j) {
                    buffer[elementIndex(indexP, j)] = data[j];
                }
                validParts[indexP] = true;
                headPart = (indexP + 1) % parts;
                assert((buffer[indexP * elementsPerPart + 0] == data[0]));
                return true;
            }
        }
        return false;
    }

    /**
     * @brief This does the same as store(), but does _not_ check on the length of the passed vector and does NOT provide mutexes (i.e. thread safety)
     * @attention This function is NOT THEAD SAFE!
     *
     * @tparam IteratorType
     * @param first
     * @param last
     * @return true
     * @return false
     */
    template<typename IteratorType>
    bool storeFast(IteratorType first, IteratorType last) {
        index_t indexP = 0;
        auto bufferSize = buffer.size();
        index_t lElementIndex = headPart * elementsPerPart;
        for (unsigned int i = 0; i < parts; ++i) {
            indexP = (headPart + i) % parts;
            if (!validParts[indexP]) {
                for (auto it = first; it != last; ++it) {
                    buffer[lElementIndex++] = *it;
                }
                validParts[indexP] = true;
                headPart = (indexP + 1) % parts;
                return true;
            }
            lElementIndex = (lElementIndex + elementsPerPart) % bufferSize;
        }
        return false;
    }

    /**
     * @brief This does the same as store(), but does _not_ check on the length of the passed vector and does NOT provide mutexes (i.e. thread safety)
     * @attention This function is NOT THEAD SAFE!
     *
     * @param data
     * @return true
     * @return false
     */
    bool storeFast(const Finn::vector<T>& data) { return storeFast(data.begin(), data.end()); }

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
     * @tparam C
     * @param outData
     * @return true
     * @return false
     */
    template<typename IteratorType>
    bool read(IteratorType outputIt) {
        index_t indexP = 0;
        for (unsigned int i = 0; i < parts; ++i) {
            indexP = (readPart + i) % parts;
            if (validParts[indexP]) {
                //! Only now set mutex. Even if the spot just became free during the loop we'll take it, but now data has to be preserved.
                std::lock_guard<std::mutex> guardPartMutex(*partMutexes[indexP]);
                std::lock_guard<std::mutex> guardHeadMutex(headPartMutex);
                for (size_t j = 0; j < elementsPerPart; ++j, ++outputIt) {
                    *outputIt = buffer[elementIndex(indexP, j)];
                }
                validParts[indexP] = false;
                readPart = (indexP + 1) % parts;
                assert((outputIt[0] == buffer[indexP * elementsPerPart + 0]));
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Read the ring buffer and write out the valid entries into the provided storage container. If no valid part is found, false is returned
     *
     * @tparam C
     * @param outData
     * @return true
     * @return false
     */
    template<typename IteratorType>
    bool readAllValidParts(IteratorType outputIt, uint valuesPerElement) {
        index_t indexP = 0;
        bool ret = false;
        for (unsigned int i = 0; i < parts; ++i) {
            indexP = (readPart + i) % parts;
            if (validParts[indexP]) {
                //! Only now set mutex. Even if the spot just became free during the loop we'll take it, but now data has to be preserved.
                std::lock_guard<std::mutex> guardPartMutex(*partMutexes[indexP]);
                std::lock_guard<std::mutex> guardHeadMutex(headPartMutex);
                for (size_t j = 0; j < valuesPerElement; ++j, ++outputIt) {
                    *outputIt = buffer[elementIndex(indexP, j)];
                }
                validParts[indexP] = false;
                readPart = (indexP + 1) % parts;
                assert((outputIt[0] == buffer[indexP * elementsPerPart + 0]));
                ret = true;
            }
        }
        return ret;
    }


#ifdef UNITTEST
     public:
    Finn::vector<T> testGetAsVector(index_t partIndex) {
        Finn::vector<T> temp;
        for (size_t i = 0; i < elementsPerPart; i++) {
            temp.emplace_back(buffer[elementIndex(partIndex, i)]);
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

#endif  // RINGBUFFER_HPP