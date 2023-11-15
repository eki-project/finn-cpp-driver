/**
 * @file RingBuffer.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de), Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements a wrapper for the circular buffer of boost
 * @version 2.0
 * @date 2023-11-14
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are
 * made available under the terms of the MIT license.
 *
 */

#ifndef RINGBUFFER
#define RINGBUFFER

#include <FINNCppDriver/utils/FinnUtils.h>
#include <FINNCppDriver/utils/Logger.h>

#include <algorithm>
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <numeric>
#include <span>
#include <syncstream>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Finn {
    /**
     * @brief Wrapper class for boost::circular_buffer, which handles abstraction.
     *
     * @tparam T
     */
    template<typename T, bool multiThreaded = false>
    class RingBuffer {
        finnBoost::circular_buffer<T> buffer;

        std::mutex readWriteMutex;
        std::condition_variable cv;

        std::size_t elementsPerPart;

        /**
         * @brief A small prefix to determine the source of the log write
         *
         * @return std::string
         */
        std::string static loggerPrefix() { return "[RingBuffer] "; }

         public:
        /**
         * @brief Construct a new Ring Buffer object. It's size in terms of values of
         * type T is given by pElementsPerPart * pParts. By default all parts are
         * invalid data to start with.
         *
         * @param pParts
         * @param pElementsPerPart
         */
        RingBuffer(const size_t pParts, const size_t pElementsPerPart) : buffer(pElementsPerPart * pParts), elementsPerPart(pElementsPerPart) {
            auto logger = Logger::getLogger();
            FINN_LOG(logger, loglevel::info) << "Ringbuffer initialised with " << pElementsPerPart << " Elements per Part and " << pParts << " Parts.\n";
            if (pElementsPerPart * pParts == 0) {
                FinnUtils::logAndError<std::runtime_error>("It is not possible to create a buffer of size 0!");
            }
        }

        RingBuffer(RingBuffer&& other) noexcept : buffer(std::move(other.buffer)), elementsPerPart(other.elementsPerPart) {}

        RingBuffer(const RingBuffer& other) = delete;
        virtual ~RingBuffer() = default;
        RingBuffer& operator=(RingBuffer&& other) = delete;
        RingBuffer& operator=(const RingBuffer& other) = delete;

        bool empty() const { return buffer.empty(); }
        bool full() const { return buffer.full(); }
        std::size_t freeSpace() const { return buffer.capacity() - buffer.size(); }

        /**
         *
         * @brief Return the RingBuffer's size, either in elements of T, in bytes or in parts
         *
         * @param ss
         * @return size_t
         */
        size_t size(SIZE_SPECIFIER ss) const {
            if (ss == SIZE_SPECIFIER::ELEMENTS) {
                return buffer.capacity();
            } else if (ss == SIZE_SPECIFIER::BYTES) {
                return buffer.capacity() * sizeof(T);
            } else if (ss == SIZE_SPECIFIER::PARTS) {
                return buffer.capacity() / elementsPerPart;
            } else if (ss == SIZE_SPECIFIER::ELEMENTS_PER_PART) {
                return elementsPerPart;
            } else {
                FinnUtils::logAndError<std::runtime_error>("Unknown size specifier!");
                return 0;
            }
        }

        size_t size() const { return buffer.size() / elementsPerPart; }

        /**
         * @brief Stores data in the ring buffer. In singlethreaded mode, it returns
         * false if data could not be stored and true otherwise. In multithreaded
         * mode, the method will block until data can be stored.
         *
         * @tparam IteratorType
         * @param first
         * @param last
         * @return true
         * @return false
         */
        template<typename IteratorType>
        bool store(IteratorType first, IteratorType last) {
            const std::size_t datasize = std::abs(std::distance(first, last));
            if (datasize % elementsPerPart != 0) {
                FinnUtils::logAndError<std::runtime_error>("It is not possible to store data that is not a multiple of a part! Datasize: " + std::to_string(datasize) + ", Elements per Part: " + std::to_string(elementsPerPart) + "\n");
            }
            if (datasize > buffer.capacity()) {
                FinnUtils::logAndError<std::runtime_error>("It is not possible to store more data in the buffer, than capacity available!");
            }
            if constexpr (multiThreaded) {
                // lock buffer
                std::unique_lock lk(readWriteMutex);
                if (datasize > freeSpace()) {
                    // go to sleep and wait until enough space available
                    cv.wait(lk, [&datasize, this] { return datasize <= freeSpace(); });
                }
                // put data into buffer
                buffer.insert(buffer.end(), first, last);

                // Manual unlocking is done before notifying, to avoid waking up
                // the waiting thread only to block again
                lk.unlock();
                cv.notify_one();
                return true;

            } else {
                if (datasize > freeSpace()) {
                    // Data could not be stored
                    return false;
                }
                // put data into buffer
                buffer.insert(buffer.end(), first, last);
                return true;
            }
        }

        template<typename IteratorType>
        bool store(const IteratorType data, size_t datasize) {
            return store(data, data + datasize);
        }

        bool store(const std::vector<T> vec) { return store(vec.begin(), vec.end()); }

        /**
         * @brief Read the ring buffer and write out the first valid entry into the
         * provided storage container. If no valid part is found, false is returned in
         * the singlethreaded case. While multithreading, the method blocks instead.
         *
         * @tparam IteratorType
         * @param outputIt
         * @return true
         * @return false
         */
        template<typename IteratorType>
        bool read(IteratorType outputIt) {
            if constexpr (multiThreaded) {
                // lock buffer
                std::unique_lock lk(readWriteMutex);

                if (buffer.size() < elementsPerPart) {
                    // Not enough data so block
                    // go to sleep and wait until enough data available
                    cv.wait(lk, [this] { return buffer.size() >= elementsPerPart; });
                }

                // read data
                auto begin = buffer.begin();
                std::copy(begin, begin + elementsPerPart, outputIt);
                buffer.erase(begin, begin + elementsPerPart);

                // Manual unlocking is done before notifying, to avoid waking up
                // the waiting thread only to block again
                lk.unlock();
                cv.notify_one();
                return true;

            } else {
                if (buffer.size() < elementsPerPart) {
                    // Not enough data so fail
                    return false;
                }

                auto begin = buffer.begin();
                std::copy(begin, begin + elementsPerPart, outputIt);
                buffer.erase(begin, begin + elementsPerPart);
                return true;
            }
        }

        /**
         * @brief Read the ring buffer and write out the valid entries into the
         * provided storage container. Read data is invalidated. If no valid part is found, false is returned
         *
         * @tparam IteratorType
         * @param outputIt
         * @return true
         * @return false
         */
        template<typename IteratorType>
        bool readAllValidParts(IteratorType outputIt) {
            if constexpr (multiThreaded) {
                std::unique_lock lk(readWriteMutex);
                if (buffer.empty()) {
                    return false;
                }

                std::copy(buffer.begin(), buffer.end(), outputIt);
                buffer.clear();

                // Manual unlocking is done before notifying, to avoid waking up
                // the waiting thread only to block again
                lk.unlock();
                cv.notify_one();
                return true;
            } else {
                if (buffer.empty()) {
                    return false;
                }

                std::copy(buffer.begin(), buffer.end(), outputIt);
                buffer.clear();

                return true;
            }
        }

        /**
         * @brief Read the ring buffer and write out the valid entries into the
         * provided storage container. If no valid part is found, false is returned
         *
         * @tparam IteratorType
         * @param outputIt
         * @return true
         * @return false
         */
        template<typename IteratorType>
        bool readWithoutInvalidation(IteratorType outputIt, int index = -1) {
            if constexpr (multiThreaded) {
                std::unique_lock lk(readWriteMutex);
                if (buffer.empty()) {
                    return false;
                }

                if (index == -1) {
                    std::copy(buffer.begin(), buffer.end(), outputIt);
                } else {
                    std::copy(buffer.begin() + elementsPerPart * index, buffer.begin() + elementsPerPart * (index + 1), outputIt);
                }


                // Manual unlocking is done before notifying, to avoid waking up
                // the waiting thread only to block again
                lk.unlock();
                cv.notify_one();
                return true;
            } else {
                if (buffer.empty()) {
                    return false;
                }

                if (index == -1) {
                    std::copy(buffer.begin(), buffer.end(), outputIt);
                } else {
                    std::copy(buffer.begin() + elementsPerPart * index, buffer.begin() + elementsPerPart * (index + 1), outputIt);
                    std::cout << std::distance(buffer.begin() + elementsPerPart * index, buffer.begin() + elementsPerPart * (index + 1)) << "\n";
                }

                return true;
            }
        }
    };
}  // namespace Finn


#endif  // RINGBUFFER
