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

#ifndef RINGBUFFERNEW
#define RINGBUFFERNEW

#include <FINNCppDriver/utils/FinnUtils.h>

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

        std::size_t freeBufferSpace() const { return buffer.capacity() - buffer.size(); }

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
            if (pElementsPerPart * pParts == 0) {
                FinnUtils::logAndError<std::runtime_error>("It is not possible to create a buffer of size 0!");
            }
        }

        RingBuffer(RingBuffer&& other) noexcept : buffer(std::move(other.buffer)), elementsPerPart(other.elementsPerPart) {}

        RingBuffer(const RingBuffer& other) = delete;
        virtual ~RingBuffer() = default;
        RingBuffer& operator=(RingBuffer&& other) = delete;
        RingBuffer& operator=(const RingBuffer& other) = delete;

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
                FinnUtils::logAndError<std::runtime_error>("It is not possible to store data that is not a multiple of a part!");
            }
            if (datasize > buffer.capacity()) {
                FinnUtils::logAndError<std::runtime_error>("It is not possible to store more data in the buffer, than capacity available!");
            }
            if constexpr (multiThreaded) {
                // lock buffer
                std::unique_lock lk(readWriteMutex);
                if (datasize > freeBufferSpace()) {
                    // go to sleep and wait until enough space available
                    cv.wait(lk, [&datasize, this] { return datasize <= freeBufferSpace(); });
                }
                // put data into buffer
                buffer.insert(buffer.end(), first, last);

                // Manual unlocking is done before notifying, to avoid waking up
                // the waiting thread only to block again
                lk.unlock();
                cv.notify_one();
                return true;

            } else {
                if (datasize > freeBufferSpace()) {
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
                auto rebegin = buffer.rbegin();
                std::copy((rebegin + elementsPerPart).base(), rebegin.base(), outputIt);
                buffer.erase((rebegin + elementsPerPart).base(), rebegin.base());

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

                auto rebegin = buffer.rbegin();
                std::copy((rebegin + elementsPerPart).base(), rebegin.base(), outputIt);
                buffer.erase((rebegin + elementsPerPart).base(), rebegin.base());
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
        bool readAllValidParts(IteratorType outputIt) {
            if (multiThreaded) {
                std::unique_lock lk(readWriteMutex);
                if (buffer.empty()) {
                    return false;
                }

                std::size_t elementsInBuffer = buffer.size() / elementsPerPart;

                // Important: This code performs some reordering while copying!
                for (std::size_t i = 0; i < elementsInBuffer; ++i) {
                    auto rebegin = buffer.rbegin();
                    std::copy((rebegin + elementsPerPart).base(), rebegin.base(), outputIt);
                    buffer.erase((rebegin + elementsPerPart).base(), rebegin.base());
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

                std::size_t elementsInBuffer = buffer.size() / elementsPerPart;

                // Important: This code performs some reordering while copying!
                for (std::size_t i = 0; i < elementsInBuffer; ++i) {
                    auto rebegin = buffer.rbegin();
                    std::copy((rebegin + elementsPerPart).base(), rebegin.base(), outputIt);
                    buffer.erase((rebegin + elementsPerPart).base(), rebegin.base());
                }

                return true;
            }
        }
    };
}  // namespace Finn


#endif  // RINGBUFFERNEW