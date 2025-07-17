/**
 * @file SPSCQueue.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Single-Producer, Single-Consumer lock-free queue implementation
 * @version 1.0
 * @date 2025-06-23
 *
 * @copyright Copyright (c) 2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 * This file provides a highly optimized SPSC queue implementation with
 * support for blocking and non-blocking operations, bulk transfers,
 * and various CPU-specific optimizations.
 */

#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <new>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

using namespace std::literals::chrono_literals;

// For CPU-specific optimizations
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#elif defined(__aarch64__)
    #include <arm_neon.h>
#endif

/**
 * @brief Namespace containing implementation details for the SPSCQueue
 *
 * This namespace contains various helper utilities, traits, and optimized
 * functions that support the main SPSCQueue implementation.
 */
namespace detail {
    // Make CACHE_LINE_SIZE accessible throughout the namespace
    static constexpr size_t CACHE_LINE_SIZE = 64;

    /**
     * @brief Enumeration of supported SIMD instruction sets
     *
     * Used to detect and select the appropriate SIMD implementation
     * for memory operations based on the target platform.
     */
    enum class SIMDSupport {
        None,    ///< No SIMD support
        SSE2,    ///< x86_64 baseline SIMD (128-bit)
        AVX,     ///< 256-bit SIMD instructions
        AVX2,    ///< Enhanced AVX instructions
        AVX512,  ///< 512-bit SIMD instructions
        NEON     ///< ARM SIMD instructions
    };

    /**
     * @brief Detects the available SIMD support for the current platform
     *
     * @return The highest level of SIMD support available on the current platform
     */
    inline SIMDSupport detect_simd_support() {
#if defined(__x86_64__) || defined(_M_X64)
    #if defined(__AVX512F__)
        return SIMDSupport::AVX512;
    #elif defined(__AVX2__)
        return SIMDSupport::AVX2;
    #elif defined(__AVX__)
        return SIMDSupport::AVX;
    #elif defined(__SSE2__)
        return SIMDSupport::SSE2;
    #else
        return SIMDSupport::None;
    #endif
#elif defined(__aarch64__)
        return SIMDSupport::NEON;
#else
        return SIMDSupport::None;
#endif
    }

    /**
     * @brief SIMD-optimized memory copy function
     *
     * Uses the appropriate SIMD instructions based on the target platform
     * to efficiently copy data between memory locations.
     *
     * @tparam T Type of elements to copy
     * @param dst Destination pointer (must not overlap with source)
     * @param src Source pointer
     * @param count Number of elements to copy
     */
    template<typename T>
    inline void simd_memcpy(T* __restrict dst, const T* __restrict src, size_t count) {
        static constexpr bool is_suitable_for_simd = std::is_trivially_copyable_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

        // Convert to byte pointers for SIMD operations
        char* d = reinterpret_cast<char*>(dst);
        const char* s = reinterpret_cast<const char*>(src);
        const size_t bytes = count * sizeof(T);

        // For small copies or non-SIMD-friendly types, use memcpy directly
        if (!is_suitable_for_simd || bytes < 128) {
            std::memcpy(d, s, bytes);
            return;
        }

        static const SIMDSupport simd_level = detect_simd_support();

#if defined(__x86_64__) || defined(_M_X64)
        // Check if both pointers are aligned for SIMD
        const bool is_aligned = (reinterpret_cast<uintptr_t>(d) % 32 == 0) && (reinterpret_cast<uintptr_t>(s) % 32 == 0);

    #if defined(__AVX512F__)
        if (simd_level >= SIMDSupport::AVX512 && bytes >= 64) {
            // AVX-512 implementation (64-byte blocks)
            size_t i = 0;

            // Handle 64-byte blocks with AVX-512
            for (; i + 64 <= bytes; i += 64) {
                __m512i data = is_aligned ? _mm512_load_si512(reinterpret_cast<const __m512i*>(s + i)) : _mm512_loadu_si512(reinterpret_cast<const __m512i*>(s + i));

                if (is_aligned) {
                    _mm512_store_si512(reinterpret_cast<__m512i*>(d + i), data);
                } else {
                    _mm512_storeu_si512(reinterpret_cast<__m512i*>(d + i), data);
                }
            }

            // Handle remainder with standard memcpy
            if (i < bytes) {
                std::memcpy(d + i, s + i, bytes - i);
            }
            return;
        }
    #endif

    #if defined(__AVX2__) || defined(__AVX__)
        if (simd_level >= SIMDSupport::AVX && bytes >= 32) {
            // AVX/AVX2 implementation (32-byte blocks)
            size_t i = 0;

            // Handle 32-byte blocks with AVX
            for (; i + 32 <= bytes; i += 32) {
                __m256i data = is_aligned ? _mm256_load_si256(reinterpret_cast<const __m256i*>(s + i)) : _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));

                if (is_aligned) {
                    _mm256_store_si256(reinterpret_cast<__m256i*>(d + i), data);
                } else {
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(d + i), data);
                }
            }

            // Handle remainder with standard memcpy
            if (i < bytes) {
                std::memcpy(d + i, s + i, bytes - i);
            }
            return;
        }
    #endif

    #if defined(__SSE2__)
        if (simd_level >= SIMDSupport::SSE2 && bytes >= 16) {
            // SSE2 implementation (16-byte blocks)
            size_t i = 0;

            // Handle 16-byte blocks with SSE2
            for (; i + 16 <= bytes; i += 16) {
                __m128i data = is_aligned ? _mm_load_si128(reinterpret_cast<const __m128i*>(s + i)) : _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + i));

                if (is_aligned) {
                    _mm_store_si128(reinterpret_cast<__m128i*>(d + i), data);
                } else {
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), data);
                }
            }

            // Handle remainder with standard memcpy
            if (i < bytes) {
                std::memcpy(d + i, s + i, bytes - i);
            }
            return;
        }
    #endif

#elif defined(__aarch64__)
        if (simd_level == SIMDSupport::NEON && bytes >= 16) {
            // NEON implementation (16-byte blocks)
            size_t i = 0;

            // Handle 16-byte blocks with NEON
            for (; i + 16 <= bytes; i += 16) {
                uint8x16_t data = vld1q_u8(reinterpret_cast<const uint8_t*>(s + i));
                vst1q_u8(reinterpret_cast<uint8_t*>(d + i), data);
            }

            // Handle remainder with standard memcpy
            if (i < bytes) {
                std::memcpy(d + i, s + i, bytes - i);
            }
            return;
        }
#endif

        // Fallback to standard memcpy
        std::memcpy(d, s, bytes);
    }

    /**
     * @brief Type trait to detect smart pointer types
     *
     * @tparam T Type to check
     * @tparam Void SFINAE helper
     */
    template<typename T, typename = void>
    struct is_smart_pointer : std::false_type {};

    /**
     * @brief Specialization for detecting smart pointer types with common operations
     *
     * Detects types that have operator*, operator->, and get() methods
     * which are common for smart pointer implementations.
     *
     * @tparam T Type to check
     */
    template<typename T>
    struct is_smart_pointer<T, std::void_t<decltype(std::declval<T>().operator*()), decltype(std::declval<T>().operator->()), decltype(std::declval<T>().get())>> : std::true_type {};

    /**
     * @brief Specialization for std::weak_ptr
     *
     * @tparam T Contained type
     */
    template<typename T>
    struct is_smart_pointer<std::weak_ptr<T>> : std::true_type {};

    /**
     * @brief Helper variable template for is_smart_pointer
     *
     * @tparam T Type to check
     */
    template<typename T>
    inline constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

    /**
     * @brief Type trait to detect container-like types
     *
     * @tparam T Type to check
     * @tparam Void SFINAE helper
     */
    template<typename T, typename = void>
    struct is_container_like : std::false_type {};

    /**
     * @brief Specialization for detecting container-like types
     *
     * Detects types that have begin(), end(), and size() methods
     * which are common for container implementations.
     *
     * @tparam T Type to check
     */
    template<typename T>
    struct is_container_like<T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end()), decltype(std::declval<T>().size())>> : std::true_type {};

    /**
     * @brief Helper variable template for is_container_like
     *
     * @tparam T Type to check
     */
    template<typename T>
    inline constexpr bool is_container_like_v = is_container_like<T>::value;

    /**
     * @brief Checks if a type has custom resource management
     *
     * Detects types that have custom destructors and move operations,
     * which often indicate resource management.
     *
     * @tparam T Type to check
     */
    template<typename T>
    inline constexpr bool has_custom_resource_management_v = !std::is_trivially_destructible_v<T> && (!std::is_trivially_move_constructible_v<T> || !std::is_trivially_move_assignable_v<T>);

    /**
     * @brief Type trait to detect types with problematic move semantics
     *
     * @tparam T Type to check
     */
    template<typename T>
    struct has_problematic_move_semantics {
        /**
         * @brief Explicit list of known problematic types
         *
         * These types are known to have issues with move-then-destroy patterns
         */
        static constexpr bool explicit_list = std::is_same_v<T, std::string> || std::is_same_v<T, std::vector<bool>> ||  // vector<bool> is special
                                              false;                                                                     // Extensible for other specific cases

        /**
         * @brief Heuristic detection for potentially problematic types
         *
         * Uses type traits to identify types that might have issues
         * when moved from and then destroyed
         */
        static constexpr bool heuristic_detection = has_custom_resource_management_v<T> && is_container_like_v<T> && !std::is_trivially_copyable_v<T>;

        /**
         * @brief Combined detection result
         */
        static constexpr bool value = explicit_list || heuristic_detection;
    };

    /**
     * @brief Helper variable template for has_problematic_move_semantics
     *
     * @tparam T Type to check
     */
    template<typename T>
    inline constexpr bool has_problematic_move_semantics_v = has_problematic_move_semantics<T>::value;

    /**
     * @brief Type trait to detect types that are unsafe to destroy after moving from
     *
     * @tparam T Type to check
     */
    template<typename T>
    struct unsafe_to_destroy_after_move : std::bool_constant<std::is_pointer_v<T> || is_smart_pointer_v<T> || has_problematic_move_semantics_v<T>> {};

    /**
     * @brief Helper variable template for unsafe_to_destroy_after_move
     *
     * @tparam T Type to check
     */
    template<typename T>
    inline constexpr bool unsafe_to_destroy_after_move_v = unsafe_to_destroy_after_move<T>::value;

    /**
     * @brief Prefetches memory for read access
     *
     * Provides a hint to the CPU to prefetch memory into cache
     * for upcoming read operations.
     *
     * @param ptr Pointer to memory to prefetch
     * @param locality Temporal locality hint (0-3, where 3 means high locality)
     */
    static inline void prefetch_read(const void* ptr, int locality = 3) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(ptr, 0, locality);  // Read with configurable locality
#endif
    }

    /**
     * @brief Prefetches memory for write access
     *
     * Provides a hint to the CPU to prefetch memory into cache
     * for upcoming write operations.
     *
     * @param ptr Pointer to memory to prefetch
     * @param locality Temporal locality hint (0-3, where 3 means high locality)
     */
    static inline void prefetch_write(const void* ptr, int locality = 3) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(ptr, 1, locality);  // Write with configurable locality
#endif
    }

    /**
     * @brief Executes a CPU pause instruction
     *
     * Used in spin-wait loops to reduce power consumption and
     * improve performance on hyper-threaded processors.
     */
    static inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
        asm volatile("or 27,27,27" ::: "memory");
#else
        std::this_thread::yield();  // Fallback to standard yield
#endif
    }

    /**
     * @brief Implements an exponential backoff strategy for spin-waiting
     *
     * Gradually increases the delay between retries to reduce contention
     * and power consumption during spin-waiting.
     */
    class exponential_backoff {
         private:
        int current_delay = 1;  ///< Current delay count
        const int max_delay;    ///< Maximum delay limit

         public:
        /**
         * @brief Constructs an exponential backoff object
         *
         * @param max Maximum delay value (default: 1024)
         */
        explicit exponential_backoff(int max = 1024) : max_delay(max) {}

        /**
         * @brief Executes the backoff delay and increases the delay for next time
         */
        void operator()() noexcept {
            for (int i = 0; i < current_delay; ++i) {
                cpu_pause();
            }

            // Exponentially increase delay, up to max_delay
            current_delay = std::min(current_delay * 2, max_delay);
        }

        /**
         * @brief Resets the delay back to initial value
         */
        void reset() noexcept { current_delay = 1; }
    };

    /**
     * @brief Common base template for SPSC queue implementations
     *
     * Provides the core functionality for both static and dynamic queue variants.
     *
     * @tparam T Element type
     * @tparam IndexMask Type of mask for index wrapping (size_t for dynamic, integral constant for static)
     * @tparam BufferAccessor Type that provides access to the underlying buffer
     * @tparam IsTrivial Whether T is a trivially copyable type
     */
    template<typename T, typename IndexMask, typename BufferAccessor, bool IsTrivial>
    class SPSCQueueBase {
         protected:
        // Buffer access through composition
        BufferAccessor buffer_;

        // Cache-aligned elements to prevent false sharing
        alignas(CACHE_LINE_SIZE) struct AlignedAtomicSize {
            std::atomic<size_t> value{0};  ///< The atomic value
            /// Padding to fill a complete cache line
            char padding[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];

            /**
             * @brief Loads the current value
             *
             * @param order Memory order for the operation
             * @return Current value
             */
            size_t load(std::memory_order order = std::memory_order_seq_cst) const noexcept { return value.load(order); }

            /**
             * @brief Stores a new value
             *
             * @param desired Value to store
             * @param order Memory order for the operation
             */
            void store(size_t desired, std::memory_order order = std::memory_order_seq_cst) noexcept { value.store(desired, order); }
        };

        AlignedAtomicSize head_;              ///< Consumer position
        char head_padding_[CACHE_LINE_SIZE];  ///< Extra padding between head and tail

        AlignedAtomicSize tail_;              ///< Producer position
        char tail_padding_[CACHE_LINE_SIZE];  ///< Extra padding after tail

        /**
         * @brief State for blocking operations
         */
        alignas(CACHE_LINE_SIZE) struct BlockingState {
            mutable std::mutex mutex_;           ///< Mutex for blocking operations
            std::condition_variable not_full_;   ///< CV for space available notifications
            std::condition_variable not_empty_;  ///< CV for item available notifications
            std::atomic<bool> is_active_{true};  ///< Whether the queue is active
            char padding[CACHE_LINE_SIZE];       ///< Padding to fill a complete cache line
        } blocking_;

        static constexpr int SPIN_ATTEMPTS = 1000;  ///< Number of spin attempts before blocking
        static constexpr int YIELD_ATTEMPTS = 50;   ///< Number of yield attempts during spinning

        // Provide access to the index mask - store by value, not by reference
        IndexMask index_mask_;

        /**
         * @brief Calculates the number of items available for consumption
         *
         * @return Number of items available
         */
        size_t available_items() const noexcept {
            const size_t head = head_.load(std::memory_order_relaxed);
            const size_t tail = tail_.load(std::memory_order_acquire);
            return (tail - head) & static_cast<size_t>(index_mask_);
        }

        /**
         * @brief Calculates the available space for production
         *
         * @return Number of free slots available
         */
        size_t available_space() const noexcept {
            const size_t head = head_.load(std::memory_order_acquire);
            const size_t tail = tail_.load(std::memory_order_relaxed);
            return ((head - tail - 1) & static_cast<size_t>(index_mask_));
        }

        /**
         * @brief Constructs the base queue
         *
         * @param index_mask Mask for index wrapping
         * @param buffer Buffer accessor (using move semantics to handle non-copyable element types)
         */
        SPSCQueueBase(IndexMask index_mask, BufferAccessor&& buffer) : buffer_(std::move(buffer)), index_mask_(index_mask) {}
    };

    /**
     * @brief Static buffer accessor for fixed-size queues
     *
     * @tparam T Element type
     * @tparam Capacity Capacity of the buffer
     */
    template<typename T, size_t Capacity>
    class StaticBufferAccessor {
         private:
        alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_;

         public:
        T& operator[](size_t index) noexcept { return buffer_[index]; }

        const T& operator[](size_t index) const noexcept { return buffer_[index]; }
    };

    /**
     * @brief Dynamic buffer accessor for runtime-sized queues
     *
     * @tparam T Element type
     */
    template<typename T>
    class DynamicBufferAccessor {
         private:
        std::unique_ptr<T, std::function<void(T*)>> buffer_;

         public:
        explicit DynamicBufferAccessor(size_t capacity) {
            // Calculate total size needed
            size_t size_bytes = capacity * sizeof(T);

            // Round up to the next multiple of CACHE_LINE_SIZE
            size_t aligned_size = (size_bytes + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);

            void* raw_memory = std::aligned_alloc(CACHE_LINE_SIZE, aligned_size);
            if (!raw_memory)
                throw std::bad_alloc();

            buffer_ = std::unique_ptr<T, std::function<void(T*)>>(static_cast<T*>(raw_memory), [](T* ptr) {
                // Cleanup will be handled by the queue class
                std::free(ptr);
            });
        }

        T& operator[](size_t index) noexcept { return buffer_.get()[index]; }

        const T& operator[](size_t index) const noexcept { return buffer_.get()[index]; }
    };

    /**
     * @brief Constant integral wrapper for static index masks
     */
    template<size_t Value>
    struct StaticIndexMask {
        constexpr operator size_t() const noexcept { return Value; }
    };
}  // namespace detail

/**
 * @brief Single-Producer Single-Consumer lock-free queue with static storage
 *
 * A high-performance queue designed for the single-producer,
 * single-consumer scenario. Features include:
 * - Lock-free operations for high throughput
 * - Blocking and non-blocking interfaces
 * - Bulk transfer operations
 * - SIMD-optimized memory operations
 * - Cache-friendly design to minimize false sharing
 * - Support for stop tokens for cancellation
 *
 * @tparam T Element type (must be movable)
 * @tparam RequestedCapacity Desired minimum capacity
 */
template<typename T, size_t RequestedCapacity>
    requires std::movable<T>
class SPSCQueue : private detail::SPSCQueueBase<T, detail::StaticIndexMask<std::bit_ceil(RequestedCapacity) - 1>, detail::StaticBufferAccessor<T, std::bit_ceil(RequestedCapacity)>, std::is_trivially_copyable_v<T>> {
     private:
    using ActualCapacityValue = std::integral_constant<size_t, std::bit_ceil(RequestedCapacity)>;
    static constexpr size_t ActualCapacity = ActualCapacityValue::value;
    using IndexMask = detail::StaticIndexMask<ActualCapacity - 1>;
    using BufferAccessor = detail::StaticBufferAccessor<T, ActualCapacity>;
    using Base = detail::SPSCQueueBase<T, IndexMask, BufferAccessor, std::is_trivially_copyable_v<T>>;

    // Import base members into this scope
    using Base::blocking_;
    using Base::buffer_;
    using Base::head_;
    using Base::index_mask_;
    using Base::SPIN_ATTEMPTS;
    using Base::tail_;
    using Base::YIELD_ATTEMPTS;

     public:
    /**
     * @brief Constructs an empty queue
     *
     * Initializes an empty queue with the specified capacity.
     * The actual capacity will be rounded up to the next power of 2,
     * with one slot reserved for implementation purposes.
     */
    constexpr SPSCQueue() noexcept : Base(IndexMask{}, BufferAccessor{}) { static_assert(ActualCapacity >= 2, "Queue capacity must be at least 2"); }

    /**
     * @brief Destructor
     *
     * Wakes up any waiting threads and properly destroys
     * any remaining elements in the queue.
     */
    ~SPSCQueue() {
        // Wake up any waiting threads and destroy remaining elements
        blocking_.is_active_.store(false, std::memory_order_release);
        blocking_.not_empty_.notify_all();
        blocking_.not_full_.notify_all();

        // Clean up any remaining elements if not trivially destructible
        if constexpr (!std::is_trivially_destructible_v<T>) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t tail = tail_.load(std::memory_order_relaxed);

            while (head != tail) {
                buffer_[head].~T();
                head = (head + 1) & static_cast<size_t>(index_mask_);
            }
        }
    }

    //////////ENQUEUE OPERATIONS//////////

    /**
     * @brief Attempts to enqueue an element (copy version)
     *
     * Non-blocking operation that attempts to add an item to the queue.
     *
     * @param item Element to enqueue
     * @return true if successful, false if the queue was full
     */
    bool try_enqueue(const T& item) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Relaxed load followed by acquire if needed (optimization)
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;
        }

        // Prefetch with locality hint for next operation
        detail::prefetch_write(&buffer_[current_tail], 3);

        // For trivially copyable small types, direct assignment is faster than placement new
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) {
            buffer_[current_tail] = item;
        } else {
            new (&buffer_[current_tail]) T(item);
        }

        // Release memory ordering ensures visibility to consumer
        tail_.store(next_tail, std::memory_order_release);

        // Only notify if queue was empty (reduces contention)
        if (current_tail == head_.load(std::memory_order_relaxed))
            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Attempts to enqueue an element (move version)
     *
     * Non-blocking operation that attempts to add an item to the queue
     * using move semantics for better performance.
     *
     * @param item Element to enqueue
     * @return true if successful, false if the queue was full
     */
    bool try_enqueue(T&& item) noexcept(std::is_nothrow_move_constructible_v<T>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Optimization: Relaxed load first, then acquire if needed
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            // Double-check with acquire semantics
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;  // Queue is full
        }

        // Optimization: Prefetch for write to reduce cache misses
        detail::prefetch_write(&buffer_[current_tail]);

        new (&buffer_[current_tail]) T(std::move(item));
        tail_.store(next_tail, std::memory_order_release);

        // Notify consumer if queue was empty
        if (current_tail == head_.load(std::memory_order_relaxed))
            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Enqueues an element, blocking if necessary (copy version)
     *
     * Blocks the calling thread until space is available in the queue.
     *
     * @param item Element to enqueue
     */
    void enqueue(const T& item) {
        // Try fast path first
        if (try_enqueue(item))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Enqueues an element, blocking if necessary (move version)
     *
     * Blocks the calling thread until space is available in the queue.
     * Uses move semantics for better performance.
     *
     * @param item Element to enqueue
     */
    void enqueue(T&& item) {
        // Try fast path first
        if (try_enqueue(std::move(item)))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(std::move(item)) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Enqueues an element with cancellation support (copy version)
     *
     * Blocks until space is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Element to enqueue
     * @param stop_token Token that can be used to cancel the operation
     * @return true if the element was enqueued, false if cancelled
     */
    template<typename StopToken>
    bool enqueue(const T& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_enqueue(item))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until space available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &item] { return try_enqueue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if enqueue succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Enqueues an element with cancellation support (move version)
     *
     * Blocks until space is available or the operation is cancelled.
     * Uses move semantics for better performance.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Element to enqueue
     * @param stop_token Token that can be used to cancel the operation
     * @return true if the element was enqueued, false if cancelled
     */
    template<typename StopToken>
    bool enqueue(T&& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_enqueue(std::move(item)))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(std::move(item)) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if enqueue succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Attempts to enqueue multiple elements in a single operation
     *
     * Non-blocking operation that attempts to add multiple items to the queue.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt>
    size_t try_enqueue_bulk(InputIt first, size_t count) noexcept {
        if (count == 0)
            return 0;

        // Fast path with relaxed ordering first
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        // Calculate available space (optimized)
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t capacity = ActualCapacity;
        const size_t available_space = (head + capacity - current_tail - 1) & index_mask_;

        if (available_space == 0)
            return 0;

        // Calculate actual amount to copy
        const size_t to_copy = std::min(available_space, count);
        const bool was_empty = (current_tail == head);

        // Optimize based on whether the enqueue wraps around the buffer
        const size_t first_chunk = std::min(to_copy, capacity - current_tail);
        const size_t second_chunk = to_copy - first_chunk;

        // Use the fastest copy method based on type
        if constexpr (std::is_trivially_copyable_v<T>) {
            if constexpr (std::is_pointer_v<InputIt> && std::is_same_v<std::remove_pointer_t<InputIt>, T>) {
                // Pointer to same type - use SIMD-optimized memory transfer
                // Use SIMD for first chunk
                detail::simd_memcpy(&buffer_[current_tail], first, first_chunk);

                // Handle wrap-around if needed with SIMD
                if (second_chunk > 0) {
                    detail::simd_memcpy(&buffer_[0], first + first_chunk, second_chunk);
                }
            } else {
                // Process first chunk
                auto it = first;
                for (size_t i = 0; i < first_chunk; i++) {
                    buffer_[current_tail + i] = *it++;
                }

                // Process second chunk if needed
                for (size_t i = 0; i < second_chunk; i++) {
                    buffer_[i] = *it++;
                }
            }
        } else {
            // Non-trivially copyable type - use placement new with iterator
            auto it = first;

            // Process first chunk
            for (size_t i = 0; i < first_chunk; i++) {
                new (&buffer_[current_tail + i]) T(*it++);
            }

            // Process second chunk if needed
            for (size_t i = 0; i < second_chunk; i++) {
                new (&buffer_[i]) T(*it++);
            }
        }

        // Update tail position with a single atomic operation
        tail_.store((current_tail + to_copy) & index_mask_, std::memory_order_release);

        // Only notify if queue was empty before
        if (was_empty) {
            blocking_.not_empty_.notify_one();
        }

        return to_copy;
    }

    /**
     * @brief Enqueues multiple elements, blocking if necessary
     *
     * Blocks until all elements are enqueued or the queue is shut down.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt>
    size_t enqueue_bulk(InputIt first, size_t count) {
        if (count == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_enqueued = try_enqueue_bulk(first, count);
        if (items_enqueued == count) {
            return items_enqueued;
        }

        // Advance iterator by items already enqueued
        std::advance(first, items_enqueued);
        size_t remaining = count - items_enqueued;

        // Exponential backoff spinning before falling back to mutex
        detail::exponential_backoff backoff;
        for (int i = 0; i < SPIN_ATTEMPTS; i++) {  // Try spinning a few times first
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    return items_enqueued;
                }
            }
            backoff();
        }

        // Fall back to mutex-based waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        while (items_enqueued < count && blocking_.is_active_.load(std::memory_order_acquire)) {
            // Wait until space is available
            blocking_.not_full_.wait(lock, [this] { return !is_full() || !blocking_.is_active_.load(std::memory_order_acquire); });

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Critical section - minimize time with lock held
            lock.unlock();

            // Try to enqueue multiple items in one go
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);

            lock.lock();

            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    break;
                }
            }
        }

        return items_enqueued;
    }

    /**
     * @brief Enqueues multiple elements with a timeout
     *
     * Attempts to enqueue elements until the specified timeout expires.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt, typename Rep, typename Period>
    size_t enqueue_bulk_for(InputIt first, size_t count, const std::chrono::duration<Rep, Period>& timeout) {
        if (count == 0)
            return 0;

        // Track start time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Try non-blocking fast path first
        size_t items_enqueued = try_enqueue_bulk(first, count);
        if (items_enqueued == count) {
            return items_enqueued;
        }

        // Advance iterator by items already enqueued
        std::advance(first, items_enqueued);
        size_t remaining = count - items_enqueued;

        // Adaptive spinning phase - use up to 20% of timeout for spinning
        // Fix: convert both durations to microseconds for comparison
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(200));
        auto spin_end_time = start_time + spin_time;

        // Spin with exponential backoff
        detail::exponential_backoff backoff;
        while (items_enqueued < count && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    return items_enqueued;
                }

                // Reset backoff on progress
                backoff.reset();
            }
            backoff();
        }

        // Check if timeout expired during spinning
        if (std::chrono::steady_clock::now() >= end_time) {
            return items_enqueued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until space is available or timeout
            if (!blocking_.not_full_.wait_until(lock, end_time, [this] { return !is_full() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                break;  // Timeout occurred
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual enqueue operation
            lock.unlock();
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            lock.lock();

            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    break;  // All items enqueued
                }
            }

        } while (items_enqueued < count && std::chrono::steady_clock::now() < end_time && blocking_.is_active_.load(std::memory_order_acquire));

        return items_enqueued;
    }


    //////////DEQUEUE OPERATIONS//////////

    /**
     * @brief Attempts to dequeue an element
     *
     * Non-blocking operation that attempts to remove an item from the queue.
     *
     * @param item Reference to store the dequeued element
     * @return true if successful, false if the queue was empty
     */
    bool try_dequeue(T& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Early relaxed check before acquiring
        if (current_head == tail_.load(std::memory_order_relaxed)) {
            if (current_head == tail_.load(std::memory_order_acquire))
                return false;
        }

        // Prefetch next items in queue for better throughput
        const size_t next_head = (current_head + 1) & index_mask_;
        if (next_head != tail_.load(std::memory_order_relaxed)) {
            detail::prefetch_read(&buffer_[next_head], 3);

            const size_t next_next_head = (next_head + 1) & index_mask_;
            if (next_next_head != tail_.load(std::memory_order_relaxed)) {
                detail::prefetch_read(&buffer_[next_next_head], 2);  // Lower locality hint
            }
        }

        // Move the item out with optimization for trivial types
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) {
            item = buffer_[current_head];
        } else {
            item = std::move(buffer_[current_head]);

            // Only call destructor if not an unsafe type after move
            if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                buffer_[current_head].~T();
            }
        }

        // Release memory ordering ensures visibility to producer
        head_.store(next_head, std::memory_order_release);

        // Selective notification strategy
        const size_t used_capacity = ((tail_.load(std::memory_order_relaxed) - next_head) & index_mask_);
        if (used_capacity < ActualCapacity / 4) {  // Use ActualCapacity instead of buffer_size_
            blocking_.not_full_.notify_one();
        }

        return true;
    }

    /**
     * @brief Dequeues an element, blocking if necessary
     *
     * Blocks the calling thread until an item is available in the queue.
     *
     * @param item Reference to store the dequeued element
     * @return true if an element was dequeued, false if the queue was shut down
     */
    bool dequeue(T& item) {
        // Try optimistic fast path first
        if (try_dequeue(item))
            return true;

        // Use exponential backoff with CPU hints
        detail::exponential_backoff backoff;
        for (int i = 0; i < SPIN_ATTEMPTS; ++i) {
            if (try_dequeue(item))
                return true;
            backoff();
        }

        // Fall back to blocking wait
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_empty_.wait(lock, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });
        return blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Add a timed wait method with adaptive waiting
     *
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param timeout Maximum time to wait
     * @return true if an element was dequeued, false if timeout expired during spinning
     */
    template<typename Rep, typename Period>
    bool dequeue_for(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Calculate how much time to allocate for spinning vs blocking
        auto start_time = std::chrono::steady_clock::now();
        auto spin_duration = std::min(timeout / 2, std::chrono::milliseconds(1));
        auto spin_end_time = start_time + spin_duration;

        // Spin with increasing backoff until spin time elapsed
        detail::exponential_backoff backoff;
        while (std::chrono::steady_clock::now() < spin_end_time) {
            if (try_dequeue(item))
                return true;

            backoff();
        }

        // Calculate remaining time for blocking wait
        auto current_time = std::chrono::steady_clock::now();
        auto remaining = timeout - (current_time - start_time);
        if (remaining <= std::chrono::duration<Rep, Period>::zero())
            return false;  // Timeout already expired during spinning

        // Slow path with timeout
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        return blocking_.not_empty_.wait_for(lock, remaining, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); }) && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Dequeues an element with cancellation support
     *
     * Blocks until an item is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Reference to store the dequeued element
     * @param stop_token Token that can be used to cancel the operation
     * @return true if an element was dequeued, false if cancelled or queue shut down
     */
    template<typename StopToken>
    bool dequeue(T& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until item available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if we got an item or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Dequeues an element with timeout and cancellation support
     *
     * Attempts to dequeue an element, waiting up to the specified timeout
     * or until the operation is cancelled.
     *
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Reference to store the dequeued element
     * @param timeout Maximum time to wait
     * @param stop_token Token that can be used to cancel the operation
     * @return true if an element was dequeued, false otherwise
     */
    template<typename Rep, typename Period, typename StopToken>
    bool dequeue_for(T& item, const std::chrono::duration<Rep, Period>& timeout, StopToken&& stop_token) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Slow path with timeout and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until item available, timeout, queue inactive, or stop requested
        std::condition_variable_any{}.wait_for(lock, timeout, stop_token, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Return success only if we got an item (not stopped or timed out)
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire) && !is_empty();
    }

    /**
     * @brief Attempts to dequeue multiple elements in a single operation
     *
     * Non-blocking operation that attempts to remove multiple items from the queue.
     *
     * @tparam OutputIt Iterator type for destination
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt>
    size_t try_dequeue_bulk(OutputIt dest, size_t max_items) noexcept {
        // Quick empty check with relaxed ordering (fastest path)
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Use relaxed first, then acquire only if needed
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (current_head == tail) {
            tail = tail_.load(std::memory_order_acquire);
            if (current_head == tail) {
                return 0;
            }
        }

        // Calculate items to dequeue with minimal calculations
        const size_t available = (tail - current_head) & index_mask_;
        const size_t to_copy = std::min(available, max_items);

        // Optimize based on whether the dequeue wraps around the buffer
        const size_t first_chunk = std::min(to_copy, ActualCapacity - current_head);  // Use ActualCapacity instead of buffer_size_
        const size_t second_chunk = to_copy - first_chunk;

        // Prefetch the next cache lines ahead of time to reduce false sharing impact
        if (first_chunk > 1) {
            // Prefetch several cache lines ahead to minimize false sharing effects
            for (size_t i = 0; i < std::min(first_chunk, size_t(4)); i++) {
                detail::prefetch_read(&buffer_[current_head + i], 3);
            }
        }

        // Use the fastest copy method based on type and iterator
        if constexpr (std::is_trivially_copyable_v<T>) {
            if constexpr (std::is_pointer_v<OutputIt> && std::is_same_v<std::remove_pointer_t<OutputIt>, T>) {
                // Pointer to same type - use SIMD-optimized memory transfer
                // Use SIMD for first chunk
                detail::simd_memcpy(dest, &buffer_[current_head], first_chunk);

                // Handle wrap-around if needed with SIMD
                if (second_chunk > 0) {
                    detail::simd_memcpy(dest + first_chunk, &buffer_[0], second_chunk);
                }
            } else {
                // Other iterator type - use iterator operations
                std::copy_n(&buffer_[current_head], first_chunk, dest);

                if (second_chunk > 0) {
                    auto advanced_dest = dest;
                    std::advance(advanced_dest, first_chunk);
                    std::copy_n(&buffer_[0], second_chunk, advanced_dest);
                }
            }
        } else {
            // Non-trivial type - use move semantics
            for (size_t i = 0; i < first_chunk; i++) {
                *dest = std::move(buffer_[current_head + i]);
                ++dest;

                if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                    buffer_[current_head + i].~T();
                }
            }

            for (size_t i = 0; i < second_chunk; i++) {
                *dest = std::move(buffer_[i]);
                ++dest;

                if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                    buffer_[i].~T();
                }
            }
        }

        // Update head position with a single atomic operation
        head_.store((current_head + to_copy) & index_mask_, std::memory_order_release);

        // Only notify if we freed substantial space
        if (available == to_copy || to_copy > ActualCapacity / 4) {
            blocking_.not_full_.notify_one();
        }

        return to_copy;
    }

    /**
     * @brief Dequeues multiple elements
     *
     * Attempts to dequeue multiple elements.
     *
     * @tparam OutputIt Iterator type for destination
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param stoken Stop token for cancellation
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt>
    size_t dequeue_bulk(OutputIt dest, size_t max_items, std::stop_token stoken = {}) {
        if (max_items == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued == max_items) {
            return items_dequeued;
        }

        // If we got some items but not all, advance the destination iterator
        if (items_dequeued > 0) {
            std::advance(dest, items_dequeued);
            max_items -= items_dequeued;
        }

        // Spin with exponential backoff for a short time
        auto start_time = std::chrono::steady_clock::now();
        auto spin_time = std::chrono::microseconds(200);
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (items_dequeued < max_items && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    return items_dequeued;
                }
            }
            backoff();
        }

        if (stoken.stop_requested()) {
            return items_dequeued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until items are available or timeout
            while (!blocking_.not_empty_.wait_for(lock, 2000ms, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                if (stoken.stop_requested()) {
                    return false;
                }
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual dequeue operation
            lock.unlock();
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            lock.lock();

            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    break;  // All items dequeued
                }
            }

        } while (max_items > 0 && !stoken.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire));

        return items_dequeued;
    }

    /**
     * @brief Dequeues multiple elements with timeout
     *
     * Attempts to dequeue multiple elements, waiting up to the specified timeout.
     *
     * @tparam OutputIt Iterator type for destination
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt, typename Rep, typename Period>
    size_t dequeue_bulk_for(OutputIt dest, size_t max_items, const std::chrono::duration<Rep, Period>& timeout) {
        if (max_items == 0)
            return 0;

        // Track start time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued == max_items) {
            return items_dequeued;
        }

        // If we got some items but not all, advance the destination iterator
        if (items_dequeued > 0) {
            std::advance(dest, items_dequeued);
            max_items -= items_dequeued;
        }

        // Spin with exponential backoff for a short time
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(200));
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (items_dequeued < max_items && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    return items_dequeued;
                }
            }
            backoff();
        }

        // Check if timeout expired during spinning
        if (std::chrono::steady_clock::now() >= end_time) {
            return items_dequeued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until items are available or timeout
            if (!blocking_.not_empty_.wait_until(lock, end_time, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                break;  // Timeout occurred
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual dequeue operation
            lock.unlock();
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            lock.lock();

            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    break;  // All items dequeued
                }
            }

        } while (max_items > 0 && std::chrono::steady_clock::now() < end_time && blocking_.is_active_.load(std::memory_order_acquire));

        return items_dequeued;
    }

    /**
     * @brief Dequeues any available elements with timeout
     *
     * Attempts to dequeue elements, returning as soon as any are available
     * or the timeout expires.
     *
     * @tparam OutputIt Iterator type for destination
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt, typename Rep, typename Period>
    size_t dequeue_bulk_for_any(OutputIt dest, size_t max_items, const std::chrono::duration<Rep, Period>& timeout) {
        if (max_items == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued > 0) {
            return items_dequeued;  // Return immediately if any items were dequeued
        }

        // Track time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Spin with exponential backoff for a short time
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(100));
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                return batch_dequeued;  // Return immediately with any items
            }
            backoff();
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until any items are available, timeout, or queue inactive
        bool has_items = blocking_.not_empty_.wait_until(lock, end_time, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); });

        // If no items or queue shut down, return 0
        if (!has_items || !blocking_.is_active_.load(std::memory_order_acquire)) {
            return 0;
        }

        // Try to dequeue with lock released
        lock.unlock();
        return try_dequeue_bulk(dest, max_items);
    }

    //////////EMPLACE OPERATIONS//////////

    /**
     * @brief Attempts to construct an element in-place in the queue
     *
     * Non-blocking operation that attempts to construct an element
     * directly in the queue's buffer.
     *
     * @tparam Args Types of arguments to forward to the constructor
     * @param args Arguments to forward to the constructor
     * @return true if successful, false if the queue was full
     */
    template<typename... Args>
    bool try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Optimization: Relaxed load first, then acquire if needed
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            // Double-check with acquire semantics
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;  // Queue is full
        }

        // Optimization: Prefetch for write to reduce cache misses
        detail::prefetch_write(&buffer_[current_tail]);

        new (&buffer_[current_tail]) T(std::forward<Args>(args)...);
        tail_.store(next_tail, std::memory_order_release);

        // Notify if queue was empty
        if (current_tail == head_.load(std::memory_order_relaxed))
            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Constructs an element in-place in the queue, blocking if necessary
     *
     * Blocks the calling thread until space is available in the queue.
     *
     * @tparam Args Types of arguments to forward to the constructor
     * @param args Arguments to forward to the constructor
     */
    template<typename... Args>
    void emplace(Args&&... args) {
        // Try fast path first
        if (try_emplace(std::forward<Args>(args)...))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &args...] { return try_emplace(std::forward<Args>(args)...) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Constructs an element in-place with cancellation support
     *
     * Blocks until space is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @tparam Args Types of arguments to forward to the constructor
     * @param stop_token Token that can be used to cancel the operation
     * @param args Arguments to forward to the constructor
     * @return true if the element was emplaced, false if cancelled
     */
    template<typename StopToken, typename... Args>
    bool emplace(StopToken&& stop_token, Args&&... args) {
        // Try fast path first
        if (try_emplace(std::forward<Args>(args)...))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until space available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &args...] { return try_emplace(std::forward<Args>(args)...) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if emplace succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /////////////UTILITY METHODS//////////

    /**
     * @brief Checks if the queue is empty
     *
     * @return true if the queue is empty, false otherwise
     */
    bool is_empty() const noexcept { return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed); }

    /**
     * @brief Checks if the queue is full
     *
     * @return true if the queue is full, false otherwise
     */
    bool is_full() const noexcept {
        const size_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) & index_mask_;
        return next_tail == head_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if the queue is nearly empty
     *
     * Useful for making decisions about throttling or batching.
     *
     * @return true if the queue is less than 1/8 full, false otherwise
     */
    bool is_almost_empty() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t size = (tail - head) & index_mask_;
        return size < ActualCapacity / 8;
    }

    /**
     * @brief Checks if the queue is nearly full
     *
     * Useful for making decisions about throttling or batching.
     *
     * @return true if the queue is more than 7/8 full, false otherwise
     */
    bool is_almost_full() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t free = ((head - tail - 1) & index_mask_);
        return free < ActualCapacity / 8;
    }

    /**
     * @brief Gets the current number of elements in the queue
     *
     * @return Current size of the queue
     */
    size_t size() const noexcept { return (tail_.load(std::memory_order_relaxed) - head_.load(std::memory_order_relaxed)) & index_mask_; }

    /**
     * @brief Gets the capacity of the queue
     *
     * Returns the actual usable capacity, which is one less than
     * the internal buffer size due to the need to distinguish
     * between empty and full states.
     *
     * @return Maximum number of elements the queue can hold
     */
    constexpr size_t capacity() const noexcept {
        return ActualCapacity - 1;  // One slot is always kept empty
    }

    /**
     * @brief Gets the requested capacity from construction
     *
     * @return The minimum capacity requested when the queue was created
     */
    constexpr size_t requested_capacity() const noexcept { return RequestedCapacity; }

    /**
     * @brief Gets the actual capacity after power-of-2 rounding
     *
     * @return The actual capacity of the queue
     */
    constexpr size_t actual_capacity() const noexcept { return ActualCapacity - 1; }

    /**
     * @brief Shuts down the queue
     *
     * Wakes up all waiting threads and marks the queue as inactive.
     * No new blocking operations will succeed after shutdown.
     */
    void shutdown() noexcept {
        blocking_.is_active_.store(false, std::memory_order_release);
        blocking_.not_empty_.notify_all();
        blocking_.not_full_.notify_all();
    }

    /**
     * @brief Processes and removes all elements from the queue
     *
     * Efficiently drains the queue, passing each element to the provided consumer.
     *
     * @tparam Consumer Callable type that accepts T&&
     * @param consumer Function or functor to process each dequeued element
     * @return Number of elements processed
     */
    template<typename Consumer>
    size_t drain_all(Consumer&& consumer) {
        size_t count = 0;
        T item;

        // Fast path with bulk extraction when possible
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 64) {
            // For small trivial types, extract in batches for processing
            constexpr size_t BATCH_SIZE = 16;
            std::array<T, BATCH_SIZE> items;

            while (true) {
                size_t batch_count = try_dequeue_bulk(items.data(), BATCH_SIZE);
                if (batch_count == 0)
                    break;

                for (size_t i = 0; i < batch_count; i++) {
                    consumer(std::move(items[i]));
                }

                count += batch_count;
            }
        } else {
            // For larger/non-trivial types, process one-by-one
            while (try_dequeue(item)) {
                consumer(std::move(item));
                count++;
            }
        }

        return count;
    }
};

/**
 * @brief Single-Producer Single-Consumer lock-free queue with dynamic storage
 *
 * A high-performance queue designed for the single-producer,
 * single-consumer scenario with capacity determined at runtime.
 *
 * @tparam T Element type (must be movable)
 */
template<typename T>
    requires std::movable<T>
class DynamicSPSCQueue : private detail::SPSCQueueBase<T, size_t, detail::DynamicBufferAccessor<T>, std::is_trivially_copyable_v<T>> {
     private:
    using Base = detail::SPSCQueueBase<T, size_t, detail::DynamicBufferAccessor<T>, std::is_trivially_copyable_v<T>>;

    // Import base members into this scope
    using Base::blocking_;
    using Base::buffer_;
    using Base::head_;
    using Base::index_mask_;
    using Base::SPIN_ATTEMPTS;
    using Base::tail_;
    using Base::YIELD_ATTEMPTS;

    size_t buffer_size_;         // Actual size of the buffer (power of 2)
    size_t requested_capacity_;  // Original requested capacity

     public:
    /**
     * @brief Constructs an empty queue with dynamic size
     *
     * @param requested_capacity Desired minimum capacity (will be rounded up to next power of 2)
     */
    explicit DynamicSPSCQueue(size_t requested_capacity)
        : Base(
              // Round up to the next power of 2 and create mask
              std::bit_ceil(requested_capacity) - 1,
              // Create buffer with the calculated size
              detail::DynamicBufferAccessor<T>(std::bit_ceil(requested_capacity))),
          buffer_size_(std::bit_ceil(requested_capacity)),
          requested_capacity_(requested_capacity) {
        // Ensure minimum size
        if (buffer_size_ < 2) {
            throw std::invalid_argument("Queue capacity must be at least 2");
        }
    }

    /**
     * @brief Destructor
     *
     * Wakes up any waiting threads and properly destroys
     * any remaining elements in the queue.
     */
    ~DynamicSPSCQueue() {
        // Wake up any waiting threads
        blocking_.is_active_.store(false, std::memory_order_release);
        blocking_.not_empty_.notify_all();
        blocking_.not_full_.notify_all();

        // Clean up any remaining elements if not trivially destructible
        if constexpr (!std::is_trivially_destructible_v<T>) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t tail = tail_.load(std::memory_order_relaxed);

            while (head != tail) {
                buffer_[head].~T();
                head = (head + 1) & index_mask_;
            }
        }
    }

    // Prevent copying and moving
    DynamicSPSCQueue(const DynamicSPSCQueue&) = delete;
    DynamicSPSCQueue& operator=(const DynamicSPSCQueue&) = delete;
    DynamicSPSCQueue(DynamicSPSCQueue&&) = delete;
    DynamicSPSCQueue& operator=(DynamicSPSCQueue&&) = delete;

    /**
     * @brief Attempts to enqueue an element (copy version)
     *
     * Non-blocking operation that attempts to add an item to the queue.
     *
     * @param item Element to enqueue
     * @return true if successful, false if the queue was full
     */
    bool try_enqueue(const T& item) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Relaxed load followed by acquire if needed (optimization)
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;
        }

        // Prefetch with locality hint for next operation
        detail::prefetch_write(&buffer_[current_tail], 3);

        // For trivially copyable small types, direct assignment is faster than placement new
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) {
            buffer_[current_tail] = item;
        } else {
            new (&buffer_[current_tail]) T(item);
        }

        // Release memory ordering ensures visibility to consumer
        tail_.store(next_tail, std::memory_order_release);

        // Only notify if queue was empty (reduces contention)
        if (current_tail == head_.load(std::memory_order_relaxed))
            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Attempts to enqueue an element (move version)
     *
     * Non-blocking operation that attempts to add an item to the queue
     * using move semantics for better performance.
     *
     * @param item Element to enqueue
     * @return true if successful, false if the queue was full
     */
    bool try_enqueue(T&& item) noexcept(std::is_nothrow_move_constructible_v<T>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Optimization: Relaxed load first, then acquire if needed
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            // Double-check with acquire semantics
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;  // Queue is full
        }

        // Optimization: Prefetch for write to reduce cache misses
        detail::prefetch_write(&buffer_[current_tail]);

        new (&buffer_[current_tail]) T(std::move(item));
        tail_.store(next_tail, std::memory_order_release);

        // Notify consumer if queue was empty
        if (current_tail == head_.load(std::memory_order_relaxed))

            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Enqueues an element, blocking if necessary (copy version)
     *
     * Blocks the calling thread until space is available in the queue.
     *
     * @param item Element to enqueue
     */
    void enqueue(const T& item) {
        // Try fast path first
        if (try_enqueue(item))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Enqueues an element, blocking if necessary (move version)
     *
     * Blocks the calling thread until space is available in the queue.
     * Uses move semantics for better performance.
     *
     * @param item Element to enqueue
     */
    void enqueue(T&& item) {
        // Try fast path first
        if (try_enqueue(std::move(item)))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(std::move(item)) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Enqueues an element with cancellation support (copy version)
     *
     * Blocks until space is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Element to enqueue
     * @param stop_token Token that can be used to cancel the operation
     * @return true if the element was enqueued, false if cancelled
     */
    template<typename StopToken>
    bool enqueue(const T& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_enqueue(item))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until space available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &item] { return try_enqueue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if enqueue succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Enqueues an element with cancellation support (move version)
     *
     * Blocks until space is available or the operation is cancelled.
     * Uses move semantics for better performance.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Element to enqueue
     * @param stop_token Token that can be used to cancel the operation
     * @return true if the element was enqueued, false if cancelled
     */
    template<typename StopToken>
    bool enqueue(T&& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_enqueue(std::move(item)))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &item] { return try_enqueue(std::move(item)) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if enqueue succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Attempts to enqueue multiple elements in a single operation
     *
     * Non-blocking operation that attempts to add multiple items to the queue.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt>
    size_t try_enqueue_bulk(InputIt first, size_t count) noexcept {
        if (count == 0)
            return 0;

        // Fast path with relaxed ordering first
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        // Calculate available space (optimized)
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t capacity = buffer_size_;  // Use buffer_size_ instead of ActualCapacity
        const size_t available_space = (head + capacity - current_tail - 1) & index_mask_;

        if (available_space == 0)
            return 0;

        // Calculate actual amount to copy
        const size_t to_copy = std::min(available_space, count);
        const bool was_empty = (current_tail == head);

        // Optimize based on whether the enqueue wraps around the buffer
        const size_t first_chunk = std::min(to_copy, capacity - current_tail);
        const size_t second_chunk = to_copy - first_chunk;

        // Use the fastest copy method based on type
        if constexpr (std::is_trivially_copyable_v<T>) {
            if constexpr (std::is_pointer_v<InputIt> && std::is_same_v<std::remove_pointer_t<InputIt>, T>) {
                // Pointer to same type - use SIMD-optimized memory transfer
                // Use SIMD for first chunk
                detail::simd_memcpy(&buffer_[current_tail], first, first_chunk);

                // Handle wrap-around if needed with SIMD
                if (second_chunk > 0) {
                    detail::simd_memcpy(&buffer_[0], first + first_chunk, second_chunk);
                }
            } else {
                // Process first chunk
                auto it = first;
                for (size_t i = 0; i < first_chunk; i++) {
                    buffer_[current_tail + i] = *it++;
                }

                // Process second chunk if needed
                for (size_t i = 0; i < second_chunk; i++) {
                    buffer_[i] = *it++;
                }
            }
        } else {
            // Non-trivially copyable type - use placement new with iterator
            auto it = first;

            // Process first chunk
            for (size_t i = 0; i < first_chunk; i++) {
                new (&buffer_[current_tail + i]) T(*it++);
            }

            // Process second chunk if needed
            for (size_t i = 0; i < second_chunk; i++) {
                new (&buffer_[i]) T(*it++);
            }
        }

        // Update tail position with a single atomic operation
        tail_.store((current_tail + to_copy) & index_mask_, std::memory_order_release);

        // Only notify if queue was empty before
        if (was_empty) {
            blocking_.not_empty_.notify_one();
        }

        return to_copy;
    }

    /**
     * @brief Enqueues multiple elements, blocking if necessary
     *
     * Blocks until all elements are enqueued or the queue is shut down.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt>
    size_t enqueue_bulk(InputIt first, size_t count) {
        if (count == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_enqueued = try_enqueue_bulk(first, count);
        if (items_enqueued == count) {
            return items_enqueued;
        }

        // Advance iterator by items already enqueued
        std::advance(first, items_enqueued);
        size_t remaining = count - items_enqueued;

        // Exponential backoff spinning before falling back to mutex
        detail::exponential_backoff backoff;
        for (int i = 0; i < SPIN_ATTEMPTS; i++) {  // Try spinning a few times first
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    return items_enqueued;
                }
            }
            backoff();
        }

        // Fall back to mutex-based waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        while (items_enqueued < count && blocking_.is_active_.load(std::memory_order_acquire)) {
            // Wait until space is available
            blocking_.not_full_.wait(lock, [this] { return !is_full() || !blocking_.is_active_.load(std::memory_order_acquire); });

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Critical section - minimize time with lock held
            lock.unlock();

            // Try to enqueue multiple items in one go
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);

            lock.lock();

            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    break;
                }
            }
        }

        return items_enqueued;
    }

    /**
     * @brief Enqueues multiple elements with a timeout
     *
     * Attempts to enqueue elements until the specified timeout expires.
     *
     * @tparam InputIt Iterator type pointing to elements
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param first Iterator to the first element to enqueue
     * @param count Number of elements to enqueue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully enqueued
     */
    template<typename InputIt, typename Rep, typename Period>
    size_t enqueue_bulk_for(InputIt first, size_t count, const std::chrono::duration<Rep, Period>& timeout) {
        if (count == 0)
            return 0;

        // Track start time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Try non-blocking fast path first
        size_t items_enqueued = try_enqueue_bulk(first, count);
        if (items_enqueued == count) {
            return items_enqueued;
        }

        // Advance iterator by items already enqueued
        std::advance(first, items_enqueued);
        size_t remaining = count - items_enqueued;

        // Adaptive spinning phase - use up to 20% of timeout for spinning
        // Fix: convert both durations to microseconds for comparison
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(200));
        auto spin_end_time = start_time + spin_time;

        // Spin with exponential backoff
        detail::exponential_backoff backoff;
        while (items_enqueued < count && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    return items_enqueued;
                }

                // Reset backoff on progress
                backoff.reset();
            }
            backoff();
        }

        // Check if timeout expired during spinning
        if (std::chrono::steady_clock::now() >= end_time) {
            return items_enqueued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until space is available or timeout
            if (!blocking_.not_full_.wait_until(lock, end_time, [this] { return !is_full() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                break;  // Timeout occurred
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual enqueue operation
            lock.unlock();
            size_t batch_enqueued = try_enqueue_bulk(first, remaining);
            lock.lock();

            if (batch_enqueued > 0) {
                std::advance(first, batch_enqueued);
                items_enqueued += batch_enqueued;
                remaining -= batch_enqueued;

                if (items_enqueued == count) {
                    break;  // All items enqueued
                }
            }

        } while (items_enqueued < count && std::chrono::steady_clock::now() < end_time && blocking_.is_active_.load(std::memory_order_acquire));

        return items_enqueued;
    }


    //////////DEQUEUE OPERATIONS//////////

    /**
     * @brief Attempts to dequeue an element
     *
     * Non-blocking operation that attempts to remove an item from the queue.
     *
     * @param item Reference to store the dequeued element
     * @return true if successful, false if the queue was empty
     */
    bool try_dequeue(T& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Early relaxed check before acquiring
        if (current_head == tail_.load(std::memory_order_relaxed)) {
            if (current_head == tail_.load(std::memory_order_acquire))
                return false;
        }

        // Prefetch next items in queue for better throughput
        const size_t next_head = (current_head + 1) & index_mask_;
        if (next_head != tail_.load(std::memory_order_relaxed)) {
            detail::prefetch_read(&buffer_[next_head], 3);

            const size_t next_next_head = (next_head + 1) & index_mask_;
            if (next_next_head != tail_.load(std::memory_order_relaxed)) {
                detail::prefetch_read(&buffer_[next_next_head], 2);  // Lower locality hint
            }
        }

        // Move the item out with optimization for trivial types
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) {
            item = buffer_[current_head];
        } else {
            item = std::move(buffer_[current_head]);

            // Only call destructor if not an unsafe type after move
            if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                buffer_[current_head].~T();
            }
        }

        // Release memory ordering ensures visibility to producer
        head_.store(next_head, std::memory_order_release);

        // Selective notification strategy
        const size_t used_capacity = ((tail_.load(std::memory_order_relaxed) - next_head) & index_mask_);
        if (used_capacity < buffer_size_ / 4) {
            blocking_.not_full_.notify_one();
        }

        return true;
    }

    /**
     * @brief Dequeues an element, blocking if necessary
     *
     * Blocks the calling thread until an item is available in the queue.
     *
     * @param item Reference to store the dequeued element
     * @return true if an element was dequeued, false if the queue was shut down
     */
    bool dequeue(T& item) {
        // Try optimistic fast path first
        if (try_dequeue(item))
            return true;

        // Use exponential backoff with CPU hints
        detail::exponential_backoff backoff;
        for (int i = 0; i < SPIN_ATTEMPTS; ++i) {
            if (try_dequeue(item))
                return true;
            backoff();
        }

        // Fall back to blocking wait
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_empty_.wait(lock, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });
        return blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Add a timed wait method with adaptive waiting
     *
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param timeout Maximum time to wait
     * @return true if an element was dequeued, false if timeout expired during spinning
     */
    template<typename Rep, typename Period>
    bool dequeue_for(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Calculate how much time to allocate for spinning vs blocking
        auto start_time = std::chrono::steady_clock::now();
        auto spin_duration = std::min(timeout / 2, std::chrono::milliseconds(1));
        auto spin_end_time = start_time + spin_duration;

        // Spin with increasing backoff until spin time elapsed
        detail::exponential_backoff backoff;
        while (std::chrono::steady_clock::now() < spin_end_time) {
            if (try_dequeue(item))
                return true;

            backoff();
        }

        // Calculate remaining time for blocking wait
        auto current_time = std::chrono::steady_clock::now();
        auto remaining = timeout - (current_time - start_time);
        if (remaining <= std::chrono::duration<Rep, Period>::zero())
            return false;  // Timeout already expired during spinning

        // Slow path with timeout
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        return blocking_.not_empty_.wait_for(lock, remaining, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); }) && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Dequeues an element with cancellation support
     *
     * Blocks until an item is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Reference to store the dequeued element
     * @param stop_token Token that can be used to cancel the operation
     * @return true if an element was dequeued, false if cancelled or queue shut down
     */
    template<typename StopToken>
    bool dequeue(T& item, StopToken&& stop_token) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until item available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if we got an item or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Dequeues an element with timeout and cancellation support
     *
     * Attempts to dequeue an element, waiting up to the specified timeout
     * or until the operation is cancelled.
     *
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @tparam StopToken Type meeting the StopToken concept
     * @param item Reference to store the dequeued element
     * @param timeout Maximum time to wait
     * @param stop_token Token that can be used to cancel the operation
     * @return true if an element was dequeued, false otherwise
     */
    template<typename Rep, typename Period, typename StopToken>
    bool dequeue_for(T& item, const std::chrono::duration<Rep, Period>& timeout, StopToken&& stop_token) {
        // Try fast path first
        if (try_dequeue(item))
            return true;

        // Slow path with timeout and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until item available, timeout, queue inactive, or stop requested
        std::condition_variable_any{}.wait_for(lock, timeout, stop_token, [this, &item] { return try_dequeue(item) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Return success only if we got an item (not stopped or timed out)
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire) && !is_empty();
    }

    /**
     * @brief Attempts to dequeue multiple elements in a single operation
     *
     * Non-blocking operation that attempts to remove multiple items from the queue.
     *
     * @tparam OutputIt Iterator type for destination
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt>
    size_t try_dequeue_bulk(OutputIt dest, size_t max_items) noexcept {
        // Quick empty check with relaxed ordering (fastest path)
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Use relaxed first, then acquire only if needed
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (current_head == tail) {
            tail = tail_.load(std::memory_order_acquire);
            if (current_head == tail) {
                return 0;
            }
        }

        // Calculate items to dequeue with minimal calculations
        const size_t available = (tail - current_head) & index_mask_;
        const size_t to_copy = std::min(available, max_items);

        // Optimize based on whether the dequeue wraps around the buffer
        const size_t first_chunk = std::min(to_copy, buffer_size_ - current_head);  // Use buffer_size_ instead of ActualCapacity
        const size_t second_chunk = to_copy - first_chunk;

        // Prefetch the next cache lines ahead of time to reduce false sharing impact
        if (first_chunk > 1) {
            // Prefetch several cache lines ahead to minimize false sharing effects
            for (size_t i = 0; i < std::min(first_chunk, size_t(4)); i++) {
                detail::prefetch_read(&buffer_[current_head + i], 3);
            }
        }

        // Use the fastest copy method based on type and iterator
        if constexpr (std::is_trivially_copyable_v<T>) {
            if constexpr (std::is_pointer_v<OutputIt> && std::is_same_v<std::remove_pointer_t<OutputIt>, T>) {
                // Pointer to same type - use SIMD-optimized memory transfer
                // Use SIMD for first chunk
                detail::simd_memcpy(dest, &buffer_[current_head], first_chunk);

                // Handle wrap-around if needed with SIMD
                if (second_chunk > 0) {
                    detail::simd_memcpy(dest + first_chunk, &buffer_[0], second_chunk);
                }
            } else {
                // Other iterator type - use iterator operations
                std::copy_n(&buffer_[current_head], first_chunk, dest);

                if (second_chunk > 0) {
                    auto advanced_dest = dest;
                    std::advance(advanced_dest, first_chunk);
                    std::copy_n(&buffer_[0], second_chunk, advanced_dest);
                }
            }
        } else {
            // Non-trivial type - use move semantics
            for (size_t i = 0; i < first_chunk; i++) {
                *dest = std::move(buffer_[current_head + i]);
                ++dest;

                if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                    buffer_[current_head + i].~T();
                }
            }

            for (size_t i = 0; i < second_chunk; i++) {
                *dest = std::move(buffer_[i]);
                ++dest;

                if constexpr (!detail::unsafe_to_destroy_after_move_v<T>) {
                    buffer_[i].~T();
                }
            }
        }

        // Update head position with a single atomic operation
        head_.store((current_head + to_copy) & index_mask_, std::memory_order_release);

        // Only notify if we freed substantial space
        if (available == to_copy || to_copy > buffer_size_ / 4) {
            blocking_.not_full_.notify_one();
        }

        return to_copy;
    }

    /**
     * @brief Dequeues multiple elements
     *
     * Attempts to dequeue multiple elements.
     *
     * @tparam OutputIt Iterator type for destination
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param stoken Stop token for cancellation
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt>
    size_t dequeue_bulk(OutputIt dest, size_t max_items, std::stop_token stoken = {}) {
        if (max_items == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued == max_items) {
            return items_dequeued;
        }

        // If we got some items but not all, advance the destination iterator
        if (items_dequeued > 0) {
            std::advance(dest, items_dequeued);
            max_items -= items_dequeued;
        }

        // Spin with exponential backoff for a short time
        auto start_time = std::chrono::steady_clock::now();
        auto spin_time = std::chrono::microseconds(200);
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (items_dequeued < max_items && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    return items_dequeued;
                }
            }
            backoff();
        }

        if (stoken.stop_requested()) {
            return items_dequeued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until items are available or timeout
            while (!blocking_.not_empty_.wait_for(lock, 2000ms, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                if (stoken.stop_requested()) {
                    return false;
                }
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual dequeue operation
            lock.unlock();
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            lock.lock();

            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    break;  // All items dequeued
                }
            }

        } while (max_items > 0 && !stoken.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire));

        return items_dequeued;
    }

    /**
     * @brief Dequeues multiple elements with timeout
     *
     * Attempts to dequeue multiple elements, waiting up to the specified timeout.
     *
     * @tparam OutputIt Iterator type for destination
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt, typename Rep, typename Period>
    size_t dequeue_bulk_for(OutputIt dest, size_t max_items, const std::chrono::duration<Rep, Period>& timeout) {
        if (max_items == 0)
            return 0;

        // Track start time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued == max_items) {
            return items_dequeued;
        }

        // If we got some items but not all, advance the destination iterator
        if (items_dequeued > 0) {
            std::advance(dest, items_dequeued);
            max_items -= items_dequeued;
        }

        // Spin with exponential backoff for a short time
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(200));
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (items_dequeued < max_items && std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    return items_dequeued;
                }
            }
            backoff();
        }

        // Check if timeout expired during spinning
        if (std::chrono::steady_clock::now() >= end_time) {
            return items_dequeued;
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        do {
            // Wait until items are available or timeout
            if (!blocking_.not_empty_.wait_until(lock, end_time, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); })) {
                break;  // Timeout occurred
            }

            if (!blocking_.is_active_.load(std::memory_order_acquire)) {
                break;  // Queue was shut down
            }

            // Release lock during actual dequeue operation
            lock.unlock();
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            lock.lock();

            if (batch_dequeued > 0) {
                std::advance(dest, batch_dequeued);
                items_dequeued += batch_dequeued;
                max_items -= batch_dequeued;

                if (max_items == 0) {
                    break;  // All items dequeued
                }
            }

        } while (max_items > 0 && std::chrono::steady_clock::now() < end_time && blocking_.is_active_.load(std::memory_order_acquire));

        return items_dequeued;
    }

    /**
     * @brief Dequeues any available elements with timeout
     *
     * Attempts to dequeue elements, returning as soon as any are available
     * or the timeout expires.
     *
     * @tparam OutputIt Iterator type for destination
     * @tparam Rep Duration representation type
     * @tparam Period Duration period type
     * @param dest Iterator to the destination to store dequeued elements
     * @param max_items Maximum number of elements to dequeue
     * @param timeout Maximum time to wait
     * @return Number of elements successfully dequeued
     */
    template<typename OutputIt, typename Rep, typename Period>
    size_t dequeue_bulk_for_any(OutputIt dest, size_t max_items, const std::chrono::duration<Rep, Period>& timeout) {
        if (max_items == 0)
            return 0;

        // Try non-blocking fast path first
        size_t items_dequeued = try_dequeue_bulk(dest, max_items);
        if (items_dequeued > 0) {
            return items_dequeued;  // Return immediately if any items were dequeued
        }

        // Track time for timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        // Spin with exponential backoff for a short time
        auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        auto spin_time = std::min(timeout_us / 5, std::chrono::microseconds(100));
        auto spin_end_time = start_time + spin_time;

        detail::exponential_backoff backoff;
        while (std::chrono::steady_clock::now() < spin_end_time) {
            size_t batch_dequeued = try_dequeue_bulk(dest, max_items);
            if (batch_dequeued > 0) {
                return batch_dequeued;  // Return immediately with any items
            }
            backoff();
        }

        // Fall back to condition variable waiting
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until any items are available, timeout, or queue inactive
        bool has_items = blocking_.not_empty_.wait_until(lock, end_time, [this] { return !is_empty() || !blocking_.is_active_.load(std::memory_order_acquire); });

        // If no items or queue shut down, return 0
        if (!has_items || !blocking_.is_active_.load(std::memory_order_acquire)) {
            return 0;
        }

        // Try to dequeue with lock released
        lock.unlock();
        return try_dequeue_bulk(dest, max_items);
    }

    //////////EMPLACE OPERATIONS//////////

    /**
     * @brief Attempts to construct an element in-place in the queue
     *
     * Non-blocking operation that attempts to construct an element
     * directly in the queue's buffer.
     *
     * @tparam Args Types of arguments to forward to the constructor
     * @param args Arguments to forward to the constructor
     * @return true if successful, false if the queue was full
     */
    template<typename... Args>
    bool try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & index_mask_;

        // Optimization: Relaxed load first, then acquire if needed
        if (next_tail == head_.load(std::memory_order_relaxed)) {
            // Double-check with acquire semantics
            if (next_tail == head_.load(std::memory_order_acquire))
                return false;  // Queue is full
        }

        // Optimization: Prefetch for write to reduce cache misses
        detail::prefetch_write(&buffer_[current_tail]);

        new (&buffer_[current_tail]) T(std::forward<Args>(args)...);
        tail_.store(next_tail, std::memory_order_release);

        // Notify if queue was empty
        if (current_tail == head_.load(std::memory_order_relaxed))
            blocking_.not_empty_.notify_one();

        return true;
    }

    /**
     * @brief Constructs an element in-place in the queue, blocking if necessary
     *
     * Blocks the calling thread until space is available in the queue.
     *
     * @tparam Args Types of arguments to forward to the constructor
     * @param args Arguments to forward to the constructor
     */
    template<typename... Args>
    void emplace(Args&&... args) {
        // Try fast path first
        if (try_emplace(std::forward<Args>(args)...))
            return;

        // Slow path with blocking
        std::unique_lock<std::mutex> lock(blocking_.mutex_);
        blocking_.not_full_.wait(lock, [this, &args...] { return try_emplace(std::forward<Args>(args)...) || !blocking_.is_active_.load(std::memory_order_acquire); });
    }

    /**
     * @brief Constructs an element in-place with cancellation support
     *
     * Blocks until space is available or the operation is cancelled.
     *
     * @tparam StopToken Type meeting the StopToken concept
     * @tparam Args Types of arguments to forward to the constructor
     * @param stop_token Token that can be used to cancel the operation
     * @param args Arguments to forward to the constructor
     * @return true if the element was emplaced, false if cancelled
     */
    template<typename StopToken, typename... Args>
    bool emplace(StopToken&& stop_token, Args&&... args) {
        // Try fast path first
        if (try_emplace(std::forward<Args>(args)...))
            return true;

        // Slow path with blocking and cancellation support
        std::unique_lock<std::mutex> lock(blocking_.mutex_);

        // Wait until space available, queue inactive, or stop requested
        std::condition_variable_any{}.wait(lock, stop_token, [this, &args...] { return try_emplace(std::forward<Args>(args)...) || !blocking_.is_active_.load(std::memory_order_acquire); });

        // Check if emplace succeeded or stopped
        return !stop_token.stop_requested() && blocking_.is_active_.load(std::memory_order_acquire);
    }

    /////////////UTILITY METHODS//////////

    /**
     * @brief Checks if the queue is empty
     *
     * @return true if the queue is empty, false otherwise
     */
    bool is_empty() const noexcept { return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed); }

    /**
     * @brief Checks if the queue is full
     *
     * @return true if the queue is full, false otherwise
     */
    bool is_full() const noexcept {
        const size_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) & index_mask_;
        return next_tail == head_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the current number of elements in the queue
     *
     * @return Current size of the queue
     */
    size_t size() const noexcept { return (tail_.load(std::memory_order_relaxed) - head_.load(std::memory_order_relaxed)) & index_mask_; }

    /**
     * @brief Gets the capacity of the queue
     *
     * Returns the actual usable capacity, which is one less than
     * the internal buffer size due to the need to distinguish
     * between empty and full states.
     *
     * @return Maximum number of elements the queue can hold
     */
    constexpr size_t capacity() const noexcept {
        return buffer_size_ - 1;  // One slot is always kept empty
    }

    /**
     * @brief Gets the requested capacity from construction
     *
     * @return The minimum capacity requested when the queue was created
     */
    constexpr size_t requested_capacity() const noexcept { return requested_capacity_; }

    /**
     * @brief Gets the actual capacity after power-of-2 rounding
     *
     * @return The actual capacity of the queue
     */
    constexpr size_t actual_capacity() const noexcept { return buffer_size_ - 1; }


    /**
     * @brief Shuts down the queue
     *
     * Wakes up all waiting threads and marks the queue as inactive.
     * No new blocking operations will succeed after shutdown.
     */
    void shutdown() noexcept {
        blocking_.is_active_.store(false, std::memory_order_release);
        blocking_.not_empty_.notify_all();
        blocking_.not_full_.notify_all();
    }

    // Additional methods would be implemented similarly to the original SPSCQueue
};

#endif  // SPSC_QUEUE_HPP