#include <benchmark/benchmark.h>

#include <FINNCppDriver/utils/SPSCQueue.hpp>
#include <atomic>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Benchmark for trivial type enqueue/dequeue operations
template<size_t QueueSize>
static void BM_TrivialEnqueueDequeue(benchmark::State& state) {
    SPSCQueue<int, QueueSize> queue;
    // Ensure we don't exceed queue capacity
    const size_t operations_per_iteration = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        // Enqueue phase
        size_t enqueued = 0;
        for (size_t i = 0; i < operations_per_iteration; ++i) {
            if (queue.try_enqueue(static_cast<int>(i))) {
                enqueued++;
            }
        }

        // Dequeue phase
        int item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        // Make sure we didn't lose any items
        if (enqueued != dequeued) {
            state.SkipWithError("Enqueue/dequeue count mismatch");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration) * 2);  // enqueue + dequeue
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration * sizeof(int) * 2));
}

// Benchmark for non-trivial type enqueue/dequeue operations
template<size_t QueueSize>
static void BM_NonTrivialEnqueueDequeue(benchmark::State& state) {
    SPSCQueue<std::string, QueueSize> queue;
    std::string testString = "benchmark-test-string";
    // Ensure we don't exceed queue capacity
    const size_t operations_per_iteration = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        // Enqueue phase
        size_t enqueued = 0;
        for (size_t i = 0; i < operations_per_iteration; ++i) {
            if (queue.try_enqueue(testString)) {
                enqueued++;
            }
        }

        // Dequeue phase
        std::string item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        // Make sure we didn't lose any items
        if (enqueued != dequeued) {
            state.SkipWithError("Enqueue/dequeue count mismatch");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration) * 2);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration * testString.size() * 2));
}

// Benchmark for multi-threaded producer-consumer pattern
template<size_t QueueSize, typename T>
static void BM_ProducerConsumer(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<T, QueueSize> queue;
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_produced{0};
        std::atomic<size_t> items_consumed{0};

        // Initialize value based on type
        T value;
        if constexpr (std::is_same_v<T, int>) {
            value = 42;  // For int type
        } else if constexpr (std::is_same_v<T, std::string>) {
            value = "test-string";  // For string type
        }

        // Use a smaller maximum to avoid potential deadlocks
        const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), static_cast<size_t>(state.range(1)) * 5);

        // Start timing again before creating threads
        state.ResumeTiming();

        // Producer thread - uses non-blocking enqueue to avoid deadlocks
        std::thread producer([&queue, &producer_done, &items_produced, &num_items, value]() {
            while (items_produced.load(std::memory_order_relaxed) < num_items) {
                if (queue.try_enqueue(value)) {
                    items_produced.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Small yield to prevent busy waiting
                    std::this_thread::yield();
                }
            }
            producer_done.store(true, std::memory_order_release);
        });

        // Consumer thread
        std::thread consumer([&queue, &producer_done, &items_consumed, &num_items, &items_produced]() {
            T item;
            while (items_consumed.load(std::memory_order_relaxed) < num_items) {
                if (queue.try_dequeue(item)) {
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else if (producer_done.load(std::memory_order_acquire) && items_consumed.load(std::memory_order_relaxed) >= items_produced.load(std::memory_order_relaxed)) {
                    // All items have been produced and consumed
                    break;
                } else {
                    // Small yield to prevent busy waiting
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        // Verify all items were processed
        benchmark::DoNotOptimize(items_consumed.load(std::memory_order_relaxed));

        if (items_consumed.load(std::memory_order_relaxed) != num_items) {
            state.SkipWithError("Not all items were processed");
            break;
        }
    }

    const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), static_cast<size_t>(state.range(1)) * 5);

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items) * 2);
    if constexpr (std::is_same_v<T, std::string>) {
        constexpr size_t string_size = sizeof("test-string");
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * string_size * 2));
    } else {
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * sizeof(T) * 2));
    }
}

// Benchmark for bulk dequeue operations
template<size_t QueueSize>
static void BM_BulkDequeue(benchmark::State& state) {
    SPSCQueue<int, QueueSize> queue;
    const auto bulk_size = static_cast<size_t>(state.range(1));
    std::vector<int> items(bulk_size);

    // Ensure we don't exceed queue capacity
    const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        state.PauseTiming();
        // Fill the queue
        size_t enqueued = 0;
        for (size_t i = 0; i < num_items; ++i) {
            if (queue.try_enqueue(static_cast<int>(i))) {
                enqueued++;
            }
        }
        state.ResumeTiming();

        // Dequeue in bulk
        size_t total_dequeued = 0;
        while (total_dequeued < enqueued) {
            size_t batch_size = std::min(bulk_size, enqueued - total_dequeued);
            size_t dequeued = queue.try_dequeue_bulk(items.begin(), batch_size);
            if (dequeued == 0)
                break;  // Avoid infinite loop if dequeue fails
            total_dequeued += dequeued;
        }

        if (total_dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * sizeof(int)));
}

// Benchmark comparing individual vs bulk dequeue
static void BM_IndividualVsBulkDequeue(benchmark::State& state) {
    constexpr size_t QueueSize = 1024;
    SPSCQueue<int, QueueSize> queue;
    const bool use_bulk = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const int total_items = std::min(10000, static_cast<int>(queue.capacity()));
    const int bulk_size = std::min(100, total_items);
    std::vector<int> items(static_cast<size_t>(bulk_size));

    for (auto _ : state) {
        state.PauseTiming();
        // Fill the queue
        size_t enqueued = 0;
        for (int i = 0; i < total_items; ++i) {
            if (queue.try_enqueue(i)) {
                enqueued++;
            }
        }
        state.ResumeTiming();

        size_t dequeued = 0;
        if (use_bulk) {
            // Bulk dequeue
            while (dequeued < enqueued) {
                size_t batch_size = std::min(static_cast<size_t>(bulk_size), enqueued - dequeued);
                size_t batch_dequeued = queue.try_dequeue_bulk(items.begin(), batch_size);
                if (batch_dequeued == 0)
                    break;  // Avoid infinite loop
                dequeued += batch_dequeued;
            }
        } else {
            // Individual dequeue
            int item;
            for (size_t i = 0; i < enqueued; ++i) {
                if (queue.try_dequeue(item)) {
                    dequeued++;
                } else {
                    break;  // Stop if dequeue fails
                }
            }
        }

        if (dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(total_items) * static_cast<int64_t>(sizeof(int)));
}

// Benchmark for latency measurement using std::chrono instead of cycleclock
template<size_t QueueSize>
static void BM_EnqueueDequeueLatency(benchmark::State& state) {
    SPSCQueue<int64_t, QueueSize> queue;

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        bool enq_success = queue.try_enqueue(0);

        int64_t item = 0;
        bool deq_success = queue.try_dequeue(item);

        auto end = std::chrono::high_resolution_clock::now();

        if (!enq_success || !deq_success) {
            state.SkipWithError("Enqueue or dequeue failed");
            break;
        }

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(static_cast<double>(duration.count()) / 1e9);
    }
}

// Benchmark for emplace performance
template<size_t QueueSize>
static void BM_EmplaceVsEnqueue(benchmark::State& state) {
    SPSCQueue<std::pair<int, std::string>, QueueSize> queue;
    const bool use_emplace = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const auto num_items = static_cast<int>(std::min<size_t>(static_cast<size_t>(state.range(1)), static_cast<size_t>(queue.capacity())));

    for (auto _ : state) {
        // Track successful operations
        size_t enqueued = 0;

        if (use_emplace) {
            // Use emplace
            for (int i = 0; i < num_items; ++i) {
                if (queue.try_emplace(i, "test-string")) {
                    enqueued++;
                }
            }
        } else {
            // Use regular enqueue with constructor
            for (int i = 0; i < num_items; ++i) {
                if (queue.try_enqueue(std::make_pair(i, "test-string"))) {
                    enqueued++;
                }
            }
        }

        // Dequeue all items
        std::pair<int, std::string> item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        if (dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items) * 2);
}

// Benchmark for bulk enqueue operations
template<size_t QueueSize>
static void BM_BulkEnqueue(benchmark::State& state) {
    SPSCQueue<int, QueueSize> queue;
    const auto bulk_size = static_cast<size_t>(state.range(1));
    std::vector<int> items(bulk_size);

    // Fill the items vector with test data
    for (size_t i = 0; i < bulk_size; ++i) {
        items[i] = static_cast<int>(i);
    }

    // Ensure we don't exceed queue capacity
    const size_t num_operations = std::min<size_t>(static_cast<size_t>(state.range(0)), QueueSize / bulk_size);

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue before each measurement
        int temp;
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        // Enqueue in bulk
        size_t total_enqueued = 0;
        for (size_t i = 0; i < num_operations; ++i) {
            size_t enqueued = queue.try_enqueue_bulk(items.begin(), bulk_size);
            total_enqueued += enqueued;

            // If we couldn't enqueue the full batch, break to avoid infinite loop
            if (enqueued < bulk_size)
                break;
        }

        // Make sure we dequeue everything for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        // Record how many items we processed
        benchmark::DoNotOptimize(total_enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(bulk_size * num_operations));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(bulk_size * num_operations * sizeof(int)));
}

// Benchmark comparing individual vs bulk enqueue
static void BM_IndividualVsBulkEnqueue(benchmark::State& state) {
    constexpr size_t QueueSize = 1024;
    SPSCQueue<int, QueueSize> queue;
    const bool use_bulk = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const int total_items = std::min(10000, static_cast<int>(queue.capacity()));
    const int bulk_size = std::min(100, total_items);
    std::vector<int> items(static_cast<size_t>(bulk_size));

    // Fill the items vector with test data
    for (int i = 0; i < bulk_size; ++i) {
        items[static_cast<size_t>(i)] = i;
    }

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue before each measurement
        int temp;
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        size_t enqueued = 0;
        if (use_bulk) {
            // Bulk enqueue
            for (int i = 0; i < total_items; i += bulk_size) {
                size_t batch_size = std::min(static_cast<size_t>(bulk_size), static_cast<size_t>(total_items - i));
                size_t batch_enqueued = queue.try_enqueue_bulk(items.begin(), batch_size);
                if (batch_enqueued < batch_size)
                    break;  // Stop if queue is full
                enqueued += batch_enqueued;
            }
        } else {
            // Individual enqueue
            for (int i = 0; i < total_items; ++i) {
                if (queue.try_enqueue(i)) {
                    enqueued++;
                } else {
                    break;  // Stop if queue is full
                }
            }
        }

        // Empty the queue for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        benchmark::DoNotOptimize(enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(total_items) * static_cast<int64_t>(sizeof(int)));
}

// Benchmark for blocking bulk enqueue with varying queue fullness
template<size_t QueueSize>
static void BM_BlockingBulkEnqueue(benchmark::State& state) {
    SPSCQueue<int, QueueSize> queue;

    // Pre-fill the queue to a certain percentage of capacity
    const double fill_percentage = static_cast<double>(state.range(0)) / 100.0;
    const size_t fill_count = static_cast<size_t>(static_cast<double>(queue.capacity()) * fill_percentage);

    // Calculate how many more items we can safely enqueue
    // Add 1 to ensure we have at least one item to enqueue
    const size_t max_safe_to_enqueue = std::max<size_t>(1, queue.capacity() - fill_count);
    // Limit batch size to avoid deadlocks
    const size_t batch_size = std::min<size_t>(50, max_safe_to_enqueue);

    std::vector<int> items(batch_size);

    // Fill the items vector with test data
    for (size_t i = 0; i < items.size(); ++i) {
        items[i] = static_cast<int>(i);
    }

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue
        int temp;
        while (queue.try_dequeue(temp)) {}

        // Pre-fill the queue to the specified percentage
        for (size_t i = 0; i < fill_count; ++i) {
            queue.try_enqueue(static_cast<int>(i));
        }
        state.ResumeTiming();

        // Perform blocking bulk enqueue operation with a timeout to prevent deadlocks
        size_t enqueued = queue.enqueue_bulk_for(items.begin(), items.size(), std::chrono::milliseconds(100));

        // Empty the queue for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        benchmark::DoNotOptimize(enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(batch_size * sizeof(int)));
}

// Register the benchmarks
BENCHMARK(BM_TrivialEnqueueDequeue<16>)->Range(1, 1 << 10);
BENCHMARK(BM_TrivialEnqueueDequeue<128>)->Range(1, 1 << 10);
BENCHMARK(BM_TrivialEnqueueDequeue<1024>)->Range(1, 1 << 10);

BENCHMARK(BM_NonTrivialEnqueueDequeue<16>)->Range(1, 1 << 10);
BENCHMARK(BM_NonTrivialEnqueueDequeue<128>)->Range(1, 1 << 10);
BENCHMARK(BM_NonTrivialEnqueueDequeue<1024>)->Range(1, 1 << 10);

BENCHMARK(BM_ProducerConsumer<128, int>)->Range(1000, 100000);
BENCHMARK(BM_ProducerConsumer<128, std::string>)->Range(1000, 100000);

BENCHMARK(BM_BulkDequeue<1024>)
    ->Args({10000, 1})     // Total items, bulk size of 1
    ->Args({10000, 10})    // Total items, bulk size of 10
    ->Args({10000, 50})    // Total items, bulk size of 50
    ->Args({10000, 100});  // Total items, bulk size of 100

BENCHMARK(BM_IndividualVsBulkDequeue)
    ->Arg(0)   // Use individual dequeue
    ->Arg(1);  // Use bulk dequeue

BENCHMARK(BM_EnqueueDequeueLatency<16>)->UseRealTime();
BENCHMARK(BM_EnqueueDequeueLatency<128>)->UseRealTime();
BENCHMARK(BM_EnqueueDequeueLatency<1024>)->UseRealTime();

BENCHMARK(BM_EmplaceVsEnqueue<128>)
    ->Args({0, 1000})   // Regular enqueue, 1000 items
    ->Args({1, 1000});  // Emplace, 1000 items

BENCHMARK(BM_BulkEnqueue<1024>)
    ->Args({100, 1})     // 100 operations, bulk size of 1
    ->Args({100, 10})    // 100 operations, bulk size of 10
    ->Args({100, 50})    // 100 operations, bulk size of 50
    ->Args({100, 100});  // 100 operations, bulk size of 100

BENCHMARK(BM_IndividualVsBulkEnqueue)
    ->Arg(0)   // Use individual enqueue
    ->Arg(1);  // Use bulk enqueue

BENCHMARK(BM_BlockingBulkEnqueue<128>)
    ->Arg(0)    // Queue 0% full
    ->Arg(25)   // Queue 25% full
    ->Arg(50)   // Queue 50% full
    ->Arg(75)   // Queue 75% full
    ->Arg(95);  // Queue 95% full

BENCHMARK(BM_BlockingBulkEnqueue<1024>)
    ->Arg(0)    // Queue 0% full
    ->Arg(25)   // Queue 25% full
    ->Arg(50)   // Queue 50% full
    ->Arg(75)   // Queue 75% full
    ->Arg(95);  // Queue 95% full

// Benchmark for trivial type enqueue/dequeue operations with DynamicSPSCQueue
static void BM_DynamicTrivialEnqueueDequeue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(static_cast<size_t>(state.range(1)));  // Use second range value for capacity
    // Ensure we don't exceed queue capacity
    const size_t operations_per_iteration = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        // Enqueue phase
        size_t enqueued = 0;
        for (size_t i = 0; i < operations_per_iteration; ++i) {
            if (queue.try_enqueue(static_cast<int>(i))) {
                enqueued++;
            }
        }

        // Dequeue phase
        int item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        // Make sure we didn't lose any items
        if (enqueued != dequeued) {
            state.SkipWithError("Enqueue/dequeue count mismatch");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration) * 2);  // enqueue + dequeue
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration * sizeof(int) * 2));
}

// Benchmark for non-trivial type enqueue/dequeue operations with DynamicSPSCQueue
static void BM_DynamicNonTrivialEnqueueDequeue(benchmark::State& state) {
    DynamicSPSCQueue<std::string> queue(static_cast<size_t>(state.range(1)));  // Use second range value for capacity
    std::string testString = "benchmark-test-string";
    // Ensure we don't exceed queue capacity
    const size_t operations_per_iteration = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        // Enqueue phase
        size_t enqueued = 0;
        for (size_t i = 0; i < operations_per_iteration; ++i) {
            if (queue.try_enqueue(testString)) {
                enqueued++;
            }
        }

        // Dequeue phase
        std::string item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        // Make sure we didn't lose any items
        if (enqueued != dequeued) {
            state.SkipWithError("Enqueue/dequeue count mismatch");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration) * 2);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(operations_per_iteration * testString.size() * 2));
}

// Benchmark for multi-threaded producer-consumer pattern with DynamicSPSCQueue
template<typename T>
static void BM_DynamicProducerConsumer(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        DynamicSPSCQueue<T> queue(static_cast<size_t>(state.range(1)));  // Use second range value for capacity
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_produced{0};
        std::atomic<size_t> items_consumed{0};

        // Initialize value based on type
        T value;
        if constexpr (std::is_same_v<T, int>) {
            value = 42;  // For int type
        } else if constexpr (std::is_same_v<T, std::string>) {
            value = "test-string";  // For string type
        }

        // Use a smaller maximum to avoid potential deadlocks
        const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), static_cast<size_t>(state.range(1)) * 5);

        // Start timing again before creating threads
        state.ResumeTiming();

        // Producer thread - uses non-blocking enqueue to avoid deadlocks
        std::thread producer([&queue, &producer_done, &items_produced, &num_items, value]() {
            while (items_produced.load(std::memory_order_relaxed) < num_items) {
                if (queue.try_enqueue(value)) {
                    items_produced.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Small yield to prevent busy waiting
                    std::this_thread::yield();
                }
            }
            producer_done.store(true, std::memory_order_release);
        });

        // Consumer thread
        std::thread consumer([&queue, &producer_done, &items_consumed, &num_items, &items_produced]() {
            T item;
            while (items_consumed.load(std::memory_order_relaxed) < num_items) {
                if (queue.try_dequeue(item)) {
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else if (producer_done.load(std::memory_order_acquire) && items_consumed.load(std::memory_order_relaxed) >= items_produced.load(std::memory_order_relaxed)) {
                    // All items have been produced and consumed
                    break;
                } else {
                    // Small yield to prevent busy waiting
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        // Verify all items were processed
        benchmark::DoNotOptimize(items_consumed.load(std::memory_order_relaxed));

        if (items_consumed.load(std::memory_order_relaxed) != num_items) {
            state.SkipWithError("Not all items were processed");
            break;
        }
    }

    const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), static_cast<size_t>(state.range(1)) * 5);

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items) * 2);
    if constexpr (std::is_same_v<T, std::string>) {
        constexpr size_t string_size = sizeof("test-string");
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * string_size * 2));
    } else {
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * sizeof(T) * 2));
    }
}

// Benchmark for bulk dequeue operations with DynamicSPSCQueue
static void BM_DynamicBulkDequeue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(static_cast<size_t>(state.range(2)));  // Use third range value for capacity
    const auto bulk_size = static_cast<size_t>(state.range(1));
    std::vector<int> items(bulk_size);

    // Ensure we don't exceed queue capacity
    const size_t num_items = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity());

    for (auto _ : state) {
        state.PauseTiming();
        // Fill the queue
        size_t enqueued = 0;
        for (size_t i = 0; i < num_items; ++i) {
            if (queue.try_enqueue(static_cast<int>(i))) {
                enqueued++;
            }
        }
        state.ResumeTiming();

        // Dequeue in bulk
        size_t total_dequeued = 0;
        while (total_dequeued < enqueued) {
            size_t batch_size = std::min(bulk_size, enqueued - total_dequeued);
            size_t dequeued = queue.try_dequeue_bulk(items.begin(), batch_size);
            if (dequeued == 0)
                break;  // Avoid infinite loop if dequeue fails
            total_dequeued += dequeued;
        }

        if (total_dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_items * sizeof(int)));
}

// Benchmark comparing individual vs bulk dequeue with DynamicSPSCQueue
static void BM_DynamicIndividualVsBulkDequeue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(1024);  // Fixed capacity for this test
    const bool use_bulk = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const int total_items = std::min(10000, static_cast<int>(queue.capacity()));
    const int bulk_size = std::min(100, total_items);
    std::vector<int> items(static_cast<size_t>(bulk_size));

    for (auto _ : state) {
        state.PauseTiming();
        // Fill the queue
        size_t enqueued = 0;
        for (int i = 0; i < total_items; ++i) {
            if (queue.try_enqueue(i)) {
                enqueued++;
            }
        }
        state.ResumeTiming();

        size_t dequeued = 0;
        if (use_bulk) {
            // Bulk dequeue
            while (dequeued < enqueued) {
                size_t batch_size = std::min(static_cast<size_t>(bulk_size), enqueued - dequeued);
                size_t batch_dequeued = queue.try_dequeue_bulk(items.begin(), batch_size);
                if (batch_dequeued == 0)
                    break;  // Avoid infinite loop
                dequeued += batch_dequeued;
            }
        } else {
            // Individual dequeue
            int item;
            for (size_t i = 0; i < enqueued; ++i) {
                if (queue.try_dequeue(item)) {
                    dequeued++;
                } else {
                    break;  // Stop if dequeue fails
                }
            }
        }

        if (dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(total_items) * static_cast<int64_t>(sizeof(int)));
}

// Benchmark for latency measurement with DynamicSPSCQueue
static void BM_DynamicEnqueueDequeueLatency(benchmark::State& state) {
    DynamicSPSCQueue<int64_t> queue(static_cast<size_t>(state.range(0)));  // Use range value for capacity

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        bool enq_success = queue.try_enqueue(0);

        int64_t item = 0;
        bool deq_success = queue.try_dequeue(item);

        auto end = std::chrono::high_resolution_clock::now();

        if (!enq_success || !deq_success) {
            state.SkipWithError("Enqueue or dequeue failed");
            break;
        }

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        state.SetIterationTime(static_cast<double>(duration.count()) / 1e9);
    }
}

// Benchmark for emplace performance with DynamicSPSCQueue
static void BM_DynamicEmplaceVsEnqueue(benchmark::State& state) {
    DynamicSPSCQueue<std::pair<int, std::string>> queue(static_cast<size_t>(state.range(1)));  // Use second range value for capacity
    const bool use_emplace = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const auto num_items = static_cast<int>(std::min<size_t>(static_cast<size_t>(state.range(2)), queue.capacity()));

    for (auto _ : state) {
        // Track successful operations
        size_t enqueued = 0;

        if (use_emplace) {
            // Use emplace
            for (int i = 0; i < num_items; ++i) {
                if (queue.try_emplace(i, "test-string")) {
                    enqueued++;
                }
            }
        } else {
            // Use regular enqueue with constructor
            for (int i = 0; i < num_items; ++i) {
                if (queue.try_enqueue(std::make_pair(i, "test-string"))) {
                    enqueued++;
                }
            }
        }

        // Dequeue all items
        std::pair<int, std::string> item;
        size_t dequeued = 0;
        for (size_t i = 0; i < enqueued; ++i) {
            if (queue.try_dequeue(item)) {
                dequeued++;
            }
        }

        if (dequeued != enqueued) {
            state.SkipWithError("Not all items were dequeued");
            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_items) * 2);
}

// Benchmark for bulk enqueue operations with DynamicSPSCQueue
static void BM_DynamicBulkEnqueue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(static_cast<size_t>(state.range(2)));  // Use third range value for capacity
    const auto bulk_size = static_cast<size_t>(state.range(1));
    std::vector<int> items(bulk_size);

    // Fill the items vector with test data
    for (size_t i = 0; i < bulk_size; ++i) {
        items[i] = static_cast<int>(i);
    }

    // Ensure we don't exceed queue capacity
    const size_t num_operations = std::min<size_t>(static_cast<size_t>(state.range(0)), queue.capacity() / bulk_size);

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue before each measurement
        int temp;
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        // Enqueue in bulk
        size_t total_enqueued = 0;
        for (size_t i = 0; i < num_operations; ++i) {
            size_t enqueued = queue.try_enqueue_bulk(items.begin(), bulk_size);
            total_enqueued += enqueued;

            // If we couldn't enqueue the full batch, break to avoid infinite loop
            if (enqueued < bulk_size)
                break;
        }

        // Make sure we dequeue everything for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        // Record how many items we processed
        benchmark::DoNotOptimize(total_enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(bulk_size * num_operations));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(bulk_size * num_operations * sizeof(int)));
}

// Benchmark comparing individual vs bulk enqueue with DynamicSPSCQueue
static void BM_DynamicIndividualVsBulkEnqueue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(1024);  // Fixed capacity for this test
    const bool use_bulk = state.range(0) == 1;

    // Make sure we don't exceed queue capacity
    const int total_items = std::min(10000, static_cast<int>(queue.capacity()));
    const int bulk_size = std::min(100, total_items);
    std::vector<int> items(static_cast<size_t>(bulk_size));

    // Fill the items vector with test data
    for (int i = 0; i < bulk_size; ++i) {
        items[static_cast<size_t>(i)] = i;
    }

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue before each measurement
        int temp;
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        size_t enqueued = 0;
        if (use_bulk) {
            // Bulk enqueue
            for (int i = 0; i < total_items; i += bulk_size) {
                size_t batch_size = std::min(static_cast<size_t>(bulk_size), static_cast<size_t>(total_items - i));
                size_t batch_enqueued = queue.try_enqueue_bulk(items.begin(), batch_size);
                if (batch_enqueued < batch_size)
                    break;  // Stop if queue is full
                enqueued += batch_enqueued;
            }
        } else {
            // Individual enqueue
            for (int i = 0; i < total_items; ++i) {
                if (queue.try_enqueue(i)) {
                    enqueued++;
                } else {
                    break;  // Stop if queue is full
                }
            }
        }

        // Empty the queue for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        benchmark::DoNotOptimize(enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_items));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(total_items) * static_cast<int64_t>(sizeof(int)));
}

// Benchmark for blocking bulk enqueue with varying queue fullness with DynamicSPSCQueue
static void BM_DynamicBlockingBulkEnqueue(benchmark::State& state) {
    DynamicSPSCQueue<int> queue(static_cast<size_t>(state.range(1)));  // Use second range value for capacity

    // Pre-fill the queue to a certain percentage of capacity
    const double fill_percentage = static_cast<double>(state.range(0)) / 100.0;
    const size_t fill_count = static_cast<size_t>(static_cast<double>(queue.capacity()) * fill_percentage);

    // Calculate how many more items we can safely enqueue
    // Add 1 to ensure we have at least one item to enqueue
    const size_t max_safe_to_enqueue = std::max<size_t>(1, queue.capacity() - fill_count);
    // Limit batch size to avoid deadlocks
    const size_t batch_size = std::min<size_t>(50, max_safe_to_enqueue);

    std::vector<int> items(batch_size);

    // Fill the items vector with test data
    for (size_t i = 0; i < items.size(); ++i) {
        items[i] = static_cast<int>(i);
    }

    for (auto _ : state) {
        state.PauseTiming();
        // Clear the queue
        int temp;
        while (queue.try_dequeue(temp)) {}

        // Pre-fill the queue to the specified percentage
        for (size_t i = 0; i < fill_count; ++i) {
            queue.try_enqueue(static_cast<int>(i));
        }
        state.ResumeTiming();

        // Perform blocking bulk enqueue operation with a timeout to prevent deadlocks
        size_t enqueued = queue.enqueue_bulk_for(items.begin(), items.size(), std::chrono::milliseconds(100));

        // Empty the queue for the next iteration
        state.PauseTiming();
        while (queue.try_dequeue(temp)) {}
        state.ResumeTiming();

        benchmark::DoNotOptimize(enqueued);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(batch_size * sizeof(int)));
}

// Register dynamic benchmark variants
BENCHMARK(BM_DynamicTrivialEnqueueDequeue)->Args({10, 16})->Args({100, 128})->Args({1000, 1024});

BENCHMARK(BM_DynamicNonTrivialEnqueueDequeue)->Args({10, 16})->Args({100, 128})->Args({1000, 1024});

BENCHMARK(BM_DynamicProducerConsumer<int>)->Args({1000, 128})->Args({10000, 128})->Args({100000, 1024});

BENCHMARK(BM_DynamicProducerConsumer<std::string>)->Args({1000, 128})->Args({10000, 128})->Args({100000, 1024});

BENCHMARK(BM_DynamicBulkDequeue)->Args({10000, 1, 1024})->Args({10000, 10, 1024})->Args({10000, 50, 1024})->Args({10000, 100, 1024});

BENCHMARK(BM_DynamicIndividualVsBulkDequeue)
    ->Arg(0)   // Use individual dequeue
    ->Arg(1);  // Use bulk dequeue

BENCHMARK(BM_DynamicEnqueueDequeueLatency)->Arg(16)->Arg(128)->Arg(1024)->UseRealTime();

BENCHMARK(BM_DynamicEmplaceVsEnqueue)
    ->Args({0, 128, 1000})   // Regular enqueue, capacity 128, 1000 items
    ->Args({1, 128, 1000});  // Emplace, capacity 128, 1000 items

BENCHMARK(BM_DynamicBulkEnqueue)->Args({100, 1, 1024})->Args({100, 10, 1024})->Args({100, 50, 1024})->Args({100, 100, 1024});

BENCHMARK(BM_DynamicIndividualVsBulkEnqueue)
    ->Arg(0)   // Use individual enqueue
    ->Arg(1);  // Use bulk enqueue

BENCHMARK(BM_DynamicBlockingBulkEnqueue)
    ->Args({0, 128})    // Queue 0% full, capacity 128
    ->Args({25, 128})   // Queue 25% full, capacity 128
    ->Args({50, 128})   // Queue 50% full, capacity 128
    ->Args({75, 128})   // Queue 75% full, capacity 128
    ->Args({95, 128});  // Queue 95% full, capacity 128

BENCHMARK(BM_DynamicBlockingBulkEnqueue)
    ->Args({0, 1024})    // Queue 0% full, capacity 1024
    ->Args({25, 1024})   // Queue 25% full, capacity 1024
    ->Args({50, 1024})   // Queue 50% full, capacity 1024
    ->Args({75, 1024})   // Queue 75% full, capacity 1024
    ->Args({95, 1024});  // Queue 95% full, capacity 1024

BENCHMARK_MAIN();