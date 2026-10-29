/**
 * @file SPSCQueue.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Single-Producer, Single-Consumer queue with lock-free fast paths and blocking slow paths
 * @version 2.0
 * @date 2026-10-01
 *
 * @copyright Copyright (c) 2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 * Design notes:
 * - Fixed-size ring buffer of raw storage; element lifetimes are managed explicitly (construct on enqueue, destroy on dequeue).
 * - head/tail are monotonically increasing counters; the slot is `index & mask`. One slot is kept free so that the
 *   usable capacity is `bit_ceil(requested) - 1`.
 * - Producer and consumer each keep a private cache of the other side's index, so the shared index cache lines are
 *   only touched when the cached value is insufficient.
 * - Bulk transfers of trivially copyable elements between the ring and contiguous ranges use a single memcpy per
 *   contiguous chunk.
 * - Blocking operations spin briefly and then sleep on a condition variable. Lost wake-ups are prevented by a
 *   Dekker-style handshake: a sleeper publishes a "waiting" flag and the other side checks it after every publish.
 *   The fence that the handshake needs is made asymmetric with membarrier(2) on Linux, so the per-operation cost
 *   on the fast path is only a compiler barrier and an uncontended load.
 */

#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    #include <immintrin.h>
#endif

// Define FINN_SPSC_DISABLE_MEMBARRIER project-wide (never per translation unit) to force the full-fence fallback
#if defined(__linux__) && !defined(FINN_SPSC_DISABLE_MEMBARRIER)
    #include <linux/membarrier.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #if defined(__NR_membarrier)
        #define FINN_SPSC_USE_MEMBARRIER 1
    #endif
#endif

/**
 * @brief Implementation details of the SPSC queues
 */
namespace detail {
    /// Separation between producer- and consumer-owned data. Two cache lines, because x86 prefetches line pairs.
    inline constexpr std::size_t kFalseSharingRange = 128;

    /**
     * @brief CPU hint for spin-wait loops
     */
    inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#endif
    }

    /**
     * @brief Registers the process for expedited private membarriers (once per process)
     *
     * @return true if heavy_fence() can use membarrier(2), false if both fence sides must use full fences
     */
    inline bool membarrier_available() noexcept {
#if defined(FINN_SPSC_USE_MEMBARRIER)
        static const bool available = [] {
            if (syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0) != 0) {
                return false;
            }
            return syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0) == 0;
        }();
        return available;
#else
        return false;
#endif
    }

    /**
     * @brief Fast side of the asymmetric fence pair, executed after every publish
     *
     * Together with heavy_fence() this guarantees that either the publishing thread observes the waiting flag of a
     * sleeper, or the sleeper observes the published index. With membarrier(2) only the compiler has to keep the
     * store to published before the load of flag. Both sides must be passed the same use_membarrier value.
     *
     * @param published Index that was just stored
     * @param flag Waiting flag that is loaded next
     * @param use_membarrier Result of membarrier_available()
     */
    template<typename Published, typename Flag>
    inline void light_fence([[maybe_unused]] Published& published, [[maybe_unused]] Flag& flag, bool use_membarrier) noexcept {
        if (use_membarrier) [[likely]] {
#if defined(__GNUC__)
            // Orders exactly these two accesses; unlike a signal fence it does not force unrelated values out of registers
            asm volatile("" : "+m"(*reinterpret_cast<char(*)[sizeof(Published)]>(&published)), "+m"(*reinterpret_cast<char(*)[sizeof(Flag)]>(&flag)));
#else
            std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
            return;
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    /**
     * @brief Slow side of the asymmetric fence pair, executed by a thread before it goes to sleep
     *
     * @param use_membarrier Result of membarrier_available(); must match the value passed to light_fence()
     */
    inline void heavy_fence([[maybe_unused]] bool use_membarrier) noexcept {
#if defined(FINN_SPSC_USE_MEMBARRIER)
        if (use_membarrier) [[likely]] {
            syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);
            return;
        }
#endif
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    /**
     * @brief memcpy for trivially copyable elements without the library call for empty and single-element copies
     */
    template<typename T>
    inline void copy_elements(T* dst, const T* src, std::size_t n) noexcept {
        if (n == 1) {
            std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), sizeof(T));
        } else if (n > 1) {
            std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T));
        }
    }

    /**
     * @brief Source ranges that can be copied into the ring with memcpy
     */
    template<typename It, typename T>
    concept MemcpySource = std::is_trivially_copyable_v<T> && std::contiguous_iterator<It> && std::is_same_v<std::iter_value_t<It>, T>;

    /**
     * @brief Destination ranges that can be filled from the ring with memcpy
     */
    template<typename It, typename T>
    concept MemcpySink = std::is_trivially_copyable_v<T> && std::contiguous_iterator<It> && std::is_same_v<std::iter_reference_t<It>, T&>;

    /**
     * @brief Ring storage embedded in the queue object
     *
     * @tparam T Element type
     * @tparam Slots Number of slots (power of two)
     */
    template<typename T, std::size_t Slots>
    class StaticStorage {
        static_assert(Slots >= 2 && std::has_single_bit(Slots), "SPSCQueue capacity must be at least 2");

        alignas(alignof(T) > kFalseSharingRange ? alignof(T) : kFalseSharingRange) std::byte bytes_[Slots * sizeof(T)];

         public:
        /**
         * @brief Number of slots in the ring
         */
        static constexpr std::size_t slots() noexcept { return Slots; }

        /**
         * @brief Start of the slot array
         */
        T* data() noexcept { return reinterpret_cast<T*>(bytes_); }
    };

    /**
     * @brief Heap-allocated ring storage with a size chosen at runtime
     *
     * @tparam T Element type
     */
    template<typename T>
    class DynamicStorage {
        static constexpr std::align_val_t kAlignment{alignof(T) > kFalseSharingRange ? alignof(T) : kFalseSharingRange};

        struct Deleter {
            void operator()(T* ptr) const noexcept { ::operator delete(static_cast<void*>(ptr), kAlignment); }
        };

        std::size_t slots_;
        std::unique_ptr<T, Deleter> data_;

        static T* allocate(std::size_t slots) {
            if (slots > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
                throw std::length_error("SPSC queue capacity too large");
            }
            return static_cast<T*>(::operator new(slots * sizeof(T), kAlignment));
        }

         public:
        /**
         * @brief Allocates uninitialized storage for the given number of slots
         *
         * @param slots Number of slots (power of two, at least 2)
         */
        explicit DynamicStorage(std::size_t slots) : slots_(slots), data_(allocate(slots)) {}

        /**
         * @brief Number of slots in the ring
         */
        std::size_t slots() const noexcept { return slots_; }

        /**
         * @brief Start of the slot array
         */
        T* data() noexcept { return data_.get(); }
    };

    /**
     * @brief Shared implementation of SPSCQueue and DynamicSPSCQueue
     *
     * Exactly one thread may act as producer (enqueue/emplace operations) and exactly one thread as consumer
     * (dequeue, clear). size(), is_empty(), is_full(), capacity() and shutdown() may be called from any thread.
     *
     * Blocking operations return when they completed, when the queue is shut down, when the passed stop token is
     * triggered or when their timeout expires. Non-blocking operations keep working after shutdown, so remaining
     * elements can still be drained.
     *
     * @tparam T Element type
     * @tparam Storage Ring storage policy
     */
    template<typename T, typename Storage>
        requires std::movable<T>
    class SPSCQueueCore {
        using Clock = std::chrono::steady_clock;
        static constexpr Clock::time_point kNoDeadline = Clock::time_point::max();
        static constexpr int kSpinIterations = 256;  ///< Busy-wait rounds before a blocking call goes to sleep

         public:
        using value_type = T;  ///< Element type

        SPSCQueueCore(const SPSCQueueCore&) = delete;
        SPSCQueueCore(SPSCQueueCore&&) = delete;
        SPSCQueueCore& operator=(const SPSCQueueCore&) = delete;
        SPSCQueueCore& operator=(SPSCQueueCore&&) = delete;

        //////////ENQUEUE OPERATIONS (producer thread)//////////

        /**
         * @brief Constructs an element in place if there is space
         *
         * @param args Constructor arguments; they are only consumed on success
         * @return true if the element was enqueued, false if the queue was full
         */
        template<typename... Args>
        bool try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
            const std::size_t tail = tail_.load(std::memory_order_relaxed);
            if (free_slots(tail, 1) == 0) {
                return false;
            }
            std::construct_at(slot(tail), std::forward<Args>(args)...);
            publish_tail(tail + 1);
            return true;
        }

        /**
         * @brief Copies an element into the queue if there is space
         *
         * @return true if the element was enqueued, false if the queue was full
         */
        bool try_enqueue(const T& item) noexcept(std::is_nothrow_copy_constructible_v<T>) { return try_emplace(item); }

        /**
         * @brief Moves an element into the queue if there is space
         *
         * @return true if the element was enqueued, false if the queue was full (item is left untouched)
         */
        bool try_enqueue(T&& item) noexcept(std::is_nothrow_move_constructible_v<T>) { return try_emplace(std::move(item)); }

        /**
         * @brief Constructs an element in place, waiting for space if necessary
         *
         * @return true if the element was enqueued, false if the queue was shut down while waiting
         */
        template<typename... Args>
        bool emplace(Args&&... args) {
            return emplace_until(kNoDeadline, std::stop_token{}, std::forward<Args>(args)...);
        }

        /**
         * @brief Copies an element into the queue, waiting for space if necessary
         *
         * @return true if the element was enqueued, false on shutdown or stop request
         */
        bool enqueue(const T& item, const std::stop_token& stoken = {}) { return emplace_until(kNoDeadline, stoken, item); }

        /**
         * @brief Moves an element into the queue, waiting for space if necessary
         *
         * @return true if the element was enqueued, false on shutdown or stop request (item is left untouched)
         */
        bool enqueue(T&& item, const std::stop_token& stoken = {}) { return emplace_until(kNoDeadline, stoken, std::move(item)); }

        /**
         * @brief Copies an element into the queue, waiting at most timeout for space
         *
         * @return true if the element was enqueued, false on timeout, shutdown or stop request
         */
        template<typename Rep, typename Period>
        bool enqueue_for(const T& item, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            return emplace_until(deadline_after(timeout), stoken, item);
        }

        /**
         * @brief Moves an element into the queue, waiting at most timeout for space
         *
         * @return true if the element was enqueued, false on timeout, shutdown or stop request (item is left untouched)
         */
        template<typename Rep, typename Period>
        bool enqueue_for(T&& item, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            return emplace_until(deadline_after(timeout), stoken, std::move(item));
        }

        /**
         * @brief Copies as many of [first, first + count) into the queue as currently fit
         *
         * @return Number of elements enqueued
         */
        template<typename InputIt>
        std::size_t try_enqueue_bulk(InputIt first, std::size_t count) {
            return push_bulk(first, count);
        }

        /**
         * @brief Copies [first, first + count) into the queue, waiting for space as needed
         *
         * @return Number of elements enqueued; less than count only on shutdown or stop request
         */
        template<typename InputIt>
        std::size_t enqueue_bulk(InputIt first, std::size_t count, const std::stop_token& stoken = {}) {
            return enqueue_bulk_until(first, count, kNoDeadline, stoken);
        }

        /**
         * @brief Copies [first, first + count) into the queue, waiting at most timeout for space
         *
         * @return Number of elements enqueued
         */
        template<typename InputIt, typename Rep, typename Period>
        std::size_t enqueue_bulk_for(InputIt first, std::size_t count, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            return enqueue_bulk_until(first, count, deadline_after(timeout), stoken);
        }

        //////////DEQUEUE OPERATIONS (consumer thread)//////////

        /**
         * @brief Moves the oldest element into item if there is one
         *
         * @return true if an element was dequeued, false if the queue was empty
         */
        bool try_dequeue(T& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
            const std::size_t head = head_.load(std::memory_order_relaxed);
            if (filled_slots(head, 1) == 0) {
                return false;
            }
            T* src = std::launder(slot(head));
            item = std::move(*src);
            std::destroy_at(src);
            publish_head(head + 1);
            return true;
        }

        /**
         * @brief Moves the oldest element into item, waiting for one if necessary
         *
         * @return true if an element was dequeued, false on shutdown (with an empty queue) or stop request
         */
        bool dequeue(T& item, const std::stop_token& stoken = {}) { return dequeue_until(item, kNoDeadline, stoken); }

        /**
         * @brief Moves the oldest element into item, waiting at most timeout for one
         *
         * @return true if an element was dequeued, false on timeout, shutdown or stop request
         */
        template<typename Rep, typename Period>
        bool dequeue_for(T& item, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            return dequeue_until(item, deadline_after(timeout), stoken);
        }

        /**
         * @brief Moves up to max_items currently available elements to dest
         *
         * @return Number of elements dequeued
         */
        template<typename OutputIt>
        std::size_t try_dequeue_bulk(OutputIt dest, std::size_t max_items) {
            return pop_bulk(dest, max_items);
        }

        /**
         * @brief Moves max_items elements to dest, waiting for them as needed
         *
         * @return Number of elements dequeued; less than max_items only on shutdown or stop request
         */
        template<typename OutputIt>
        std::size_t dequeue_bulk(OutputIt dest, std::size_t max_items, const std::stop_token& stoken = {}) {
            return dequeue_bulk_until(dest, max_items, kNoDeadline, stoken);
        }

        /**
         * @brief Moves max_items elements to dest, waiting at most timeout for them
         *
         * @return Number of elements dequeued
         */
        template<typename OutputIt, typename Rep, typename Period>
        std::size_t dequeue_bulk_for(OutputIt dest, std::size_t max_items, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            return dequeue_bulk_until(dest, max_items, deadline_after(timeout), stoken);
        }

        /**
         * @brief Moves up to max_items elements to dest as soon as at least one is available
         *
         * @return Number of elements dequeued; 0 on timeout, shutdown or stop request
         */
        template<typename OutputIt, typename Rep, typename Period>
        std::size_t dequeue_bulk_for_any(OutputIt dest, std::size_t max_items, const std::chrono::duration<Rep, Period>& timeout, const std::stop_token& stoken = {}) {
            std::size_t dequeued = pop_bulk(dest, max_items);
            if (dequeued == 0 && max_items > 0 && wait_for_items(deadline_after(timeout), stoken)) {
                dequeued = pop_bulk(dest, max_items);
            }
            return dequeued;
        }

        /**
         * @brief Destroys all elements currently in the queue
         *
         * @return Number of elements removed
         */
        std::size_t clear() noexcept {
            const std::size_t head = head_.load(std::memory_order_relaxed);
            const std::size_t tail = tail_.load(std::memory_order_acquire);
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (std::size_t i = head; i != tail; ++i) {
                    std::destroy_at(std::launder(slot(i)));
                }
            }
            tail_cache_ = tail;
            if (tail != head) {
                publish_head(tail);
            }
            return tail - head;
        }

        //////////UTILITY METHODS//////////

        /**
         * @brief Number of elements in the queue (a snapshot when called concurrently)
         */
        std::size_t size() const noexcept {
            // Read head first: tail only grows, so tail - head cannot underflow
            const std::size_t head = head_.load(std::memory_order_acquire);
            const std::size_t tail = tail_.load(std::memory_order_acquire);
            return std::min(tail - head, capacity());
        }

        /**
         * @brief Whether the queue holds no elements
         */
        bool is_empty() const noexcept { return size() == 0; }

        /**
         * @brief Whether the queue holds capacity() elements
         */
        bool is_full() const noexcept { return size() == capacity(); }

        /**
         * @brief Maximum number of elements the queue can hold (one slot of the ring is kept free)
         */
        std::size_t capacity() const noexcept { return mask(); }

        /**
         * @brief Same as capacity(); the requested capacity rounded up to a power of two, minus one
         */
        std::size_t actual_capacity() const noexcept { return capacity(); }

        /**
         * @brief Wakes all blocked operations and makes future blocking operations return instead of waiting
         *
         * Non-blocking operations keep working, so remaining elements can still be drained.
         */
        void shutdown() {
            active_.store(false, std::memory_order_seq_cst);
            std::lock_guard<std::mutex> lock(wait_mutex_);
            not_empty_.notify_all();
            not_full_.notify_all();
        }

        /**
         * @brief Whether shutdown() has not been called yet
         */
        bool is_active() const noexcept { return active_.load(std::memory_order_acquire); }

         protected:
        /**
         * @brief Constructs the queue with the given storage
         *
         * @param args Arguments forwarded to the storage policy
         */
        template<typename... StorageArgs>
        explicit SPSCQueueCore(StorageArgs&&... args) : storage_(std::forward<StorageArgs>(args)...) {}

        /**
         * @brief Destroys remaining elements; no other thread may use the queue anymore
         */
        ~SPSCQueueCore() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                const std::size_t tail = tail_.load(std::memory_order_relaxed);
                for (std::size_t i = head_.load(std::memory_order_relaxed); i != tail; ++i) {
                    std::destroy_at(std::launder(slot(i)));
                }
            }
        }

         private:
        std::size_t mask() const noexcept { return storage_.slots() - 1; }

        T* slot(std::size_t index) noexcept { return storage_.data() + (index & mask()); }

        template<typename Rep, typename Period>
        static Clock::time_point deadline_after(const std::chrono::duration<Rep, Period>& timeout) {
            const auto now = Clock::now();
            if (timeout <= std::chrono::duration<Rep, Period>::zero()) {
                return now;
            }
            if (std::chrono::duration_cast<std::chrono::duration<double>>(timeout) >= std::chrono::duration_cast<std::chrono::duration<double>>(kNoDeadline - now)) {
                return kNoDeadline;
            }
            return now + std::chrono::ceil<Clock::duration>(timeout);
        }

        /// Producer: free slots, refreshing the cached head only if the cached value is not enough
        std::size_t free_slots(std::size_t tail, std::size_t wanted) noexcept {
            std::size_t available = mask() - (tail - head_cache_);
            if (available < wanted) {
                head_cache_ = head_.load(std::memory_order_acquire);
                available = mask() - (tail - head_cache_);
            }
            return available;
        }

        /// Consumer: filled slots, refreshing the cached tail only if the cached value is not enough
        std::size_t filled_slots(std::size_t head, std::size_t wanted) noexcept {
            std::size_t available = tail_cache_ - head;
            if (available < wanted) {
                tail_cache_ = tail_.load(std::memory_order_acquire);
                available = tail_cache_ - head;
            }
            return available;
        }

        void publish_tail(std::size_t tail) noexcept {
            tail_.store(tail, std::memory_order_release);
            light_fence(tail_, consumer_waiting_, use_membarrier_);
            if (consumer_waiting_.load(std::memory_order_relaxed)) [[unlikely]] {
                wake(not_empty_);
            }
        }

        void publish_head(std::size_t head) noexcept {
            head_.store(head, std::memory_order_release);
            light_fence(head_, producer_waiting_, use_membarrier_);
            if (producer_waiting_.load(std::memory_order_relaxed)) [[unlikely]] {
                wake(not_full_);
            }
        }

        void wake(std::condition_variable_any& cv) noexcept {
            // Taking the mutex orders this notification after the sleeper's final predicate check
            { std::lock_guard<std::mutex> lock(wait_mutex_); }
            cv.notify_one();
        }

        /// Copies n elements from it into uninitialized slots starting at dst; strong exception guarantee
        template<typename InputIt>
        static void copy_into(InputIt& it, T* dst, std::size_t n) {
            if constexpr (MemcpySource<InputIt, T>) {
                copy_elements(dst, std::to_address(it), n);
                it += static_cast<std::iter_difference_t<InputIt>>(n);
            } else {
                std::size_t i = 0;
                try {
                    for (; i < n; ++i, ++it) {
                        std::construct_at(dst + i, *it);
                    }
                } catch (...) {
                    std::destroy_n(dst, i);
                    throw;
                }
            }
        }

        template<typename InputIt>
        std::size_t push_bulk(InputIt& first, std::size_t count) {
            const std::size_t tail = tail_.load(std::memory_order_relaxed);
            const std::size_t n = std::min(count, free_slots(tail, count));
            if (n == 0) {
                return 0;
            }
            const std::size_t offset = tail & mask();
            const std::size_t first_chunk = std::min(n, storage_.slots() - offset);
            T* base = storage_.data();
            copy_into(first, base + offset, first_chunk);
            try {
                copy_into(first, base, n - first_chunk);
            } catch (...) {
                std::destroy_n(base + offset, first_chunk);
                throw;
            }
            publish_tail(tail + n);
            return n;
        }

        template<typename OutputIt>
        std::size_t pop_bulk(OutputIt& dest, std::size_t max_items) {
            const std::size_t head = head_.load(std::memory_order_relaxed);
            const std::size_t n = std::min(max_items, filled_slots(head, max_items));
            if (n == 0) {
                return 0;
            }
            if constexpr (MemcpySink<OutputIt, T>) {
                const std::size_t offset = head & mask();
                const std::size_t first_chunk = std::min(n, storage_.slots() - offset);
                T* out = std::to_address(dest);
                copy_elements(out, storage_.data() + offset, first_chunk);
                copy_elements(out + first_chunk, storage_.data(), n - first_chunk);
                dest += static_cast<std::iter_difference_t<OutputIt>>(n);
            } else {
                std::size_t done = 0;
                try {
                    for (; done < n; ++done, ++dest) {
                        T* src = std::launder(slot(head + done));
                        *dest = std::move(*src);
                        std::destroy_at(src);
                    }
                } catch (...) {
                    // Elements [head, head + done) were handed out and destroyed; the failing one stays queued
                    publish_head(head + done);
                    throw;
                }
            }
            publish_head(head + n);
            return n;
        }

        /**
         * @brief Waits until ready() holds, the queue is shut down, a stop is requested or the deadline passes
         *
         * @return The final value of ready()
         */
        template<typename Ready>
        bool wait_until_ready(Ready ready, std::atomic<bool>& waiting, std::condition_variable_any& cv, Clock::time_point deadline, const std::stop_token& stoken) {
            // Hand-offs between busy threads are far cheaper than a sleep/wake cycle, so spin briefly first
            for (int i = 0; i < kSpinIterations; ++i) {
                if (ready()) {
                    return true;
                }
                if (!active_.load(std::memory_order_relaxed) || stoken.stop_requested()) {
                    return false;
                }
                cpu_relax();
            }

            std::unique_lock<std::mutex> lock(wait_mutex_);
            waiting.store(true, std::memory_order_relaxed);
            heavy_fence(use_membarrier_);  // Pairs with light_fence() in publish_*: we see the new index or the publisher sees the flag
            const auto predicate = [&] { return ready() || !active_.load(std::memory_order_acquire); };
            if (deadline == kNoDeadline) {
                cv.wait(lock, stoken, predicate);
            } else {
                cv.wait_until(lock, stoken, deadline, predicate);
            }
            waiting.store(false, std::memory_order_relaxed);
            return ready();
        }

        bool wait_for_items(Clock::time_point deadline, const std::stop_token& stoken) {
            const auto ready = [this] { return tail_.load(std::memory_order_acquire) != head_.load(std::memory_order_relaxed); };
            return wait_until_ready(ready, consumer_waiting_, not_empty_, deadline, stoken);
        }

        bool wait_for_space(Clock::time_point deadline, const std::stop_token& stoken) {
            const auto ready = [this] { return tail_.load(std::memory_order_relaxed) - head_.load(std::memory_order_acquire) < mask(); };
            return wait_until_ready(ready, producer_waiting_, not_full_, deadline, stoken);
        }

        template<typename... Args>
        bool emplace_until(Clock::time_point deadline, const std::stop_token& stoken, Args&&... args) {
            // try_emplace only consumes args when it succeeds, so forwarding them again after a failure is safe
            while (!try_emplace(std::forward<Args>(args)...)) {
                if (!wait_for_space(deadline, stoken)) {
                    return false;
                }
            }
            return true;
        }

        bool dequeue_until(T& item, Clock::time_point deadline, const std::stop_token& stoken) {
            while (!try_dequeue(item)) {
                if (!wait_for_items(deadline, stoken)) {
                    return false;
                }
            }
            return true;
        }

        template<typename InputIt>
        std::size_t enqueue_bulk_until(InputIt& first, std::size_t count, Clock::time_point deadline, const std::stop_token& stoken) {
            std::size_t done = push_bulk(first, count);
            while (done < count && wait_for_space(deadline, stoken)) {
                done += push_bulk(first, count - done);
            }
            return done;
        }

        template<typename OutputIt>
        std::size_t dequeue_bulk_until(OutputIt& dest, std::size_t max_items, Clock::time_point deadline, const std::stop_token& stoken) {
            std::size_t done = pop_bulk(dest, max_items);
            while (done < max_items && wait_for_items(deadline, stoken)) {
                done += pop_bulk(dest, max_items - done);
            }
            return done;
        }

        // Read-only after construction (for the static variant this is also the slot array)
        Storage storage_;

        // Producer-owned: published write index and the producer's cache of the read index
        alignas(kFalseSharingRange) std::atomic<std::size_t> tail_{0};
        std::size_t head_cache_ = 0;

        // Consumer-owned: published read index and the consumer's cache of the write index
        alignas(kFalseSharingRange) std::atomic<std::size_t> head_{0};
        std::size_t tail_cache_ = 0;

        // Read on every publish, written only around sleeping, so this line stays shared-clean on the fast path
        alignas(kFalseSharingRange) std::atomic<bool> consumer_waiting_{false};
        std::atomic<bool> producer_waiting_{false};
        std::atomic<bool> active_{true};
        const bool use_membarrier_ = membarrier_available();  ///< Fixed before any thread uses the queue, so both fence sides agree

        alignas(kFalseSharingRange) std::mutex wait_mutex_;
        std::condition_variable_any not_empty_;
        std::condition_variable_any not_full_;
    };
}  // namespace detail

/**
 * @brief Single-Producer Single-Consumer queue with storage embedded in the object
 *
 * @tparam T Element type (must be movable)
 * @tparam RequestedCapacity Requested capacity; the ring has bit_ceil(RequestedCapacity) slots, one of which stays free
 */
template<typename T, std::size_t RequestedCapacity>
    requires std::movable<T>
class SPSCQueue : public detail::SPSCQueueCore<T, detail::StaticStorage<T, std::bit_ceil(RequestedCapacity)>> {
     public:
    /**
     * @brief Constructs an empty queue
     *
     * User-provided on purpose: a defaulted constructor would make `SPSCQueue<...> q{};` zero-fill the whole ring.
     */
    SPSCQueue() {}  // NOLINT(modernize-use-equals-default)

    /**
     * @brief The capacity passed as template argument
     */
    static constexpr std::size_t requested_capacity() noexcept { return RequestedCapacity; }
};

/**
 * @brief Single-Producer Single-Consumer queue with heap storage sized at runtime
 *
 * @tparam T Element type (must be movable)
 */
template<typename T>
    requires std::movable<T>
class DynamicSPSCQueue : public detail::SPSCQueueCore<T, detail::DynamicStorage<T>> {
    using Base = detail::SPSCQueueCore<T, detail::DynamicStorage<T>>;

    std::size_t requested_capacity_;

    static std::size_t slots_for(std::size_t requested_capacity) {
        if (requested_capacity < 2) {
            throw std::invalid_argument("Queue capacity must be at least 2");
        }
        if (requested_capacity > std::numeric_limits<std::size_t>::max() / 2 + 1) {
            throw std::length_error("SPSC queue capacity too large");
        }
        return std::bit_ceil(requested_capacity);
    }

     public:
    /**
     * @brief Constructs an empty queue
     *
     * @param requested_capacity Requested capacity; the ring has bit_ceil(requested_capacity) slots, one of which stays free
     */
    explicit DynamicSPSCQueue(std::size_t requested_capacity) : Base(slots_for(requested_capacity)), requested_capacity_(requested_capacity) {}

    /**
     * @brief The capacity passed to the constructor
     */
    std::size_t requested_capacity() const noexcept { return requested_capacity_; }
};

#endif  // SPSC_QUEUE_HPP
