#include <FINNCppDriver/utils/SPSCQueue.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

// Basic tests for non-trivial type
TEST(SPSCQueueTest, BasicOperations) {
    SPSCQueue<std::string, 16> queue;

    // Test empty state
    EXPECT_TRUE(queue.is_empty());
    EXPECT_FALSE(queue.is_full());
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.capacity(), 15);  // One slot always kept empty

    // Test actual capacity vs requested
    EXPECT_EQ(queue.requested_capacity(), 16);
    EXPECT_EQ(queue.actual_capacity(), 15);

    // Test enqueue and size
    EXPECT_TRUE(queue.try_enqueue("test1"));
    EXPECT_FALSE(queue.is_empty());
    EXPECT_EQ(queue.size(), 1);

    // Test dequeue
    std::string item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test1");
    EXPECT_TRUE(queue.is_empty());

    // Test multiple items
    EXPECT_TRUE(queue.try_enqueue("test2"));
    EXPECT_TRUE(queue.try_enqueue("test3"));
    EXPECT_EQ(queue.size(), 2);

    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test2");
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test3");
    EXPECT_TRUE(queue.is_empty());
}

// Test for trivially copyable type (using the specialization)
TEST(SPSCQueueTest, TrivialTypeOperations) {
    SPSCQueue<int, 16> queue;

    EXPECT_TRUE(queue.try_enqueue(42));
    EXPECT_EQ(queue.size(), 1);

    int item = 0;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, 42);
    EXPECT_TRUE(queue.is_empty());
}

// Test queue capacity and power-of-2 rounding
TEST(SPSCQueueTest, CapacityRounding) {
    // Test with non-power-of-2 capacity
    SPSCQueue<int, 10> queue1;
    EXPECT_EQ(queue1.requested_capacity(), 10);
    EXPECT_EQ(queue1.actual_capacity(), 15);  // Rounded up to 16-1

    // Test with power-of-2 capacity
    SPSCQueue<int, 16> queue2;
    EXPECT_EQ(queue2.requested_capacity(), 16);
    EXPECT_EQ(queue2.actual_capacity(), 15);  // 16-1
}

// Test filling the queue to capacity
TEST(SPSCQueueTest, FullQueue) {
    SPSCQueue<int, 4> queue;  // Actual capacity: 3

    EXPECT_TRUE(queue.try_enqueue(1));
    EXPECT_TRUE(queue.try_enqueue(2));
    EXPECT_TRUE(queue.try_enqueue(3));
    EXPECT_TRUE(queue.is_full());        // Should be full now
    EXPECT_FALSE(queue.try_enqueue(4));  // Should fail

    int item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, 1);
    EXPECT_FALSE(queue.is_full());  // No longer full

    // Can enqueue again
    EXPECT_TRUE(queue.try_enqueue(4));
}

// Test wrap-around behavior
TEST(SPSCQueueTest, WrapAround) {
    SPSCQueue<int, 4> queue;  // Actual capacity: 3
    std::vector<int> results;

    // Fill and drain multiple times to force wrap-around
    for (int cycle = 0; cycle < 3; cycle++) {
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 1));
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 2));
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 3));

        int item;
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
    }

    // Verify correct order
    std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    EXPECT_EQ(results, expected);
}

// Test move semantics
TEST(SPSCQueueTest, MoveSemantics) {
    SPSCQueue<std::unique_ptr<int>, 4> queue;

    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(43);

    EXPECT_TRUE(queue.try_enqueue(std::move(ptr1)));
    EXPECT_TRUE(queue.try_enqueue(std::move(ptr2)));

    // Original pointers should be null after move
    EXPECT_EQ(ptr1, nullptr);
    EXPECT_EQ(ptr2, nullptr);

    std::unique_ptr<int> result;
    EXPECT_TRUE(queue.try_dequeue(result));
    EXPECT_EQ(*result, 42);

    EXPECT_TRUE(queue.try_dequeue(result));
    EXPECT_EQ(*result, 43);
}

// Test emplace functionality
TEST(SPSCQueueTest, Emplace) {
    SPSCQueue<std::pair<int, std::string>, 4> queue;

    EXPECT_TRUE(queue.try_emplace(1, "one"));
    EXPECT_TRUE(queue.try_emplace(2, "two"));

    std::pair<int, std::string> item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item.first, 1);
    EXPECT_EQ(item.second, "one");

    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item.first, 2);
    EXPECT_EQ(item.second, "two");
}

// Test bulk operations
TEST(SPSCQueueTest, BulkDequeue) {
    SPSCQueue<int, 16> queue;

    // Enqueue several items
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(queue.try_enqueue(i));
    }

    // Dequeue in bulk
    std::vector<int> results(5);
    size_t count = queue.try_dequeue_bulk(results.begin(), 5);

    EXPECT_EQ(count, 5);
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(results[i], static_cast<int>(i));
    }

    // Dequeue remaining items
    count = queue.try_dequeue_bulk(results.begin(), 5);
    EXPECT_EQ(count, 5);
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(results[i], static_cast<int>(i + 5));
    }

    // Queue should be empty now
    EXPECT_TRUE(queue.is_empty());
}

// Test blocking behavior with threads
TEST(SPSCQueueTest, BlockingOperations) {
    SPSCQueue<int, 4> queue;  // Actual capacity: 3
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    std::vector<int> produced;
    std::vector<int> consumed;

    // Producer thread - will produce 10 items
    std::thread producer([&queue, &producer_done, &produced]() {
        for (int i = 0; i < 10; i++) {
            produced.push_back(i);  // No mutex needed - only producer thread touches this
            queue.enqueue(i);       // Blocking enqueue
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread - will consume all items
    std::thread consumer([&queue, &producer_done, &consumer_done, &consumed]() {
        while (true) {
            int item;
            // Use a timeout to avoid hanging indefinitely
            if (queue.dequeue_for(item, std::chrono::milliseconds(100))) {
                consumed.push_back(item);  // No mutex needed - only consumer thread touches this
            } else {
                // Check if we're done - if producer is done AND queue is empty
                if (producer_done.load(std::memory_order_acquire) && queue.is_empty()) {
                    break;
                }
                // If we timed out but aren't done, just try again
            }
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Set a timeout for the entire test
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);  // 5 second timeout should be more than enough

    while (!consumer_done.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - start_time > timeout) {
            // Test is taking too long, likely deadlocked - force shutdown
            queue.shutdown();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    producer.join();
    consumer.join();

    // Verify all items were produced and consumed in order
    EXPECT_EQ(produced, consumed);
    EXPECT_EQ(consumed.size(), 10);
}

// Test timed operations
TEST(SPSCQueueTest, TimedOperations) {
    SPSCQueue<int, 4> queue;

    // Test timeout on empty queue
    int item = -1;
    auto start = std::chrono::steady_clock::now();
    bool result = queue.dequeue_for(item, std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_FALSE(result);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test successful timed dequeue
    queue.try_enqueue(42);
    result = queue.dequeue_for(item, std::chrono::milliseconds(100));
    EXPECT_TRUE(result);
    EXPECT_EQ(item, 42);
}

// Test bulk timed operations
TEST(SPSCQueueTest, BulkTimedOperations) {
    SPSCQueue<int, 16> queue;

    // Test timeout on empty queue
    std::vector<int> results(5);
    auto start = std::chrono::steady_clock::now();
    size_t count = queue.dequeue_bulk_for(results.begin(), 5, std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(count, 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test with some items
    for (int i = 0; i < 3; i++) {
        queue.try_enqueue(i);
    }

    count = queue.dequeue_bulk_for(results.begin(), 5, std::chrono::milliseconds(100));
    EXPECT_EQ(count, 3);
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(results[i], static_cast<int>(i));
    }
}

// Test dequeue_bulk_for_any
TEST(SPSCQueueTest, BulkTimedAnyOperations) {
    SPSCQueue<int, 16> queue;

    // Test timeout on empty queue
    std::vector<int> results(5);
    auto start = std::chrono::steady_clock::now();
    size_t count = queue.dequeue_bulk_for_any(results.begin(), 5, std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(count, 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test with some items, added gradually
    std::thread producer([&queue]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.try_enqueue(42);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.try_enqueue(43);
        queue.try_enqueue(44);
    });

    results.assign(5, 0);
    count = queue.dequeue_bulk_for_any(results.begin(), 5, std::chrono::milliseconds(200));

    EXPECT_GE(count, 1);  // Should get at least the first item
    EXPECT_EQ(results[0], 42);

    if (count > 1) {
        EXPECT_EQ(results[1], 43);
    }

    producer.join();

    // Cleanup any remaining items
    queue.try_dequeue_bulk(results.begin(), 5);
}

// Test shutdown behavior
TEST(SPSCQueueTest, Shutdown) {
    SPSCQueue<int, 4> queue;
    std::atomic<bool> consumer_unblocked{false};

    // Start a consumer thread that will block
    std::thread consumer([&queue, &consumer_unblocked]() {
        int item;
        bool result = queue.dequeue(item);  // This should block
        EXPECT_FALSE(result);               // After shutdown, should return false
        consumer_unblocked = true;
    });

    // Give the consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Shutdown the queue
    queue.shutdown();

    // Consumer should unblock
    consumer.join();
    EXPECT_TRUE(consumer_unblocked);

    // After shutdown, operations should fail
    int item;
    EXPECT_FALSE(queue.dequeue(item));
    EXPECT_FALSE(queue.dequeue_for(item, std::chrono::milliseconds(1)));
}

// Test concurrent enqueue/dequeue with high throughput
TEST(SPSCQueueTest, ConcurrentThroughput) {
    for (size_t runs = 0; runs < 1000; ++runs) {
        constexpr size_t ITEM_COUNT = 1000000;
        SPSCQueue<uint64_t, 1024> queue;
        std::atomic<bool> error{false};
        std::atomic<bool> producer_done{false};

        std::thread producer([&queue, &error, &producer_done]() {
            try {
                for (uint64_t i = 0; i < ITEM_COUNT; i++) {
                    // Use non-blocking enqueue with retry
                    while (!queue.try_enqueue(i) && !error.load(std::memory_order_relaxed)) {
                        std::this_thread::yield();  // Give consumer time to catch up
                    }

                    // Exit early if consumer detected an error
                    if (error.load(std::memory_order_relaxed)) {
                        break;
                    }
                }
            } catch (...) { error = true; }
            producer_done.store(true, std::memory_order_release);
        });

        std::thread consumer([&queue, &error, &producer_done]() {
            try {
                uint64_t expected = 0;
                while (expected < ITEM_COUNT && !error.load(std::memory_order_relaxed)) {
                    uint64_t item;

                    // Use timeout-based dequeue or non-blocking with yield
                    if (queue.try_dequeue(item)) {
                        if (item != expected) {
                            error = true;
                            break;
                        }
                        expected++;
                    } else if (producer_done.load(std::memory_order_acquire) && queue.is_empty()) {
                        // Producer is done and the queue is drained, so any item not seen yet is missing
                        if (expected < ITEM_COUNT) {
                            error = true;
                        }
                        break;
                    } else {
                        std::this_thread::yield();  // Give producer time to produce
                    }
                }
            } catch (...) { error = true; }
        });

        // Set timeout for test to avoid hanging forever
        auto start_time = std::chrono::steady_clock::now();
        bool joined = false;

        // Try to join with timeout
        while (!joined && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
            producer.join();
            consumer.join();
            joined = true;
        }

        // If not joined within timeout, consider it a deadlock
        EXPECT_TRUE(joined) << "Test timed out - likely deadlock";
        EXPECT_FALSE(error) << "Data corruption or missing items detected";
    }
}

// Test bulk enqueue operations
TEST(SPSCQueueTest, BulkEnqueue) {
    SPSCQueue<int, 16> queue;  // Actual capacity: 15

    // Test basic bulk enqueue
    std::vector<int> items1 = {1, 2, 3, 4, 5};
    size_t enqueued = queue.try_enqueue_bulk(items1.begin(), items1.size());

    EXPECT_EQ(enqueued, 5);
    EXPECT_EQ(queue.size(), 5);

    // Test partial enqueue (not enough space)
    std::vector<int> items2(12, 42);  // 12 items with value 42
    enqueued = queue.try_enqueue_bulk(items2.begin(), items2.size());

    EXPECT_EQ(enqueued, 10);  // Only 10 more should fit (15 - 5 already in queue)
    EXPECT_EQ(queue.size(), 15);
    EXPECT_TRUE(queue.is_full());

    // Dequeue and verify values
    std::vector<int> results(15);
    size_t dequeued = queue.try_dequeue_bulk(results.begin(), 15);

    EXPECT_EQ(dequeued, 15);
    EXPECT_EQ(results[0], 1);  // First batch
    EXPECT_EQ(results[4], 5);
    EXPECT_EQ(results[5], 42);  // Second batch
    EXPECT_EQ(results[14], 42);

    // Test enqueue with empty queue
    std::vector<int> items3 = {10, 20, 30};
    enqueued = queue.try_enqueue_bulk(items3.begin(), items3.size());

    EXPECT_EQ(enqueued, 3);

    // Test wrap-around behavior
    queue.try_dequeue_bulk(results.begin(), 1);  // Remove one item

    std::vector<int> items4(14, 99);  // Try to add 14 items
    enqueued = queue.try_enqueue_bulk(items4.begin(), items4.size());

    EXPECT_EQ(enqueued, 13);  // Should fit 13 more (capacity 15 - 2 already there)

    // Dequeue all and verify
    dequeued = queue.try_dequeue_bulk(results.begin(), 15);

    EXPECT_EQ(dequeued, 15);
    EXPECT_EQ(results[0], 20);
    EXPECT_EQ(results[1], 30);
    EXPECT_EQ(results[2], 99);
    EXPECT_EQ(results[14], 99);
}

// Test blocking bulk enqueue
TEST(SPSCQueueTest, BlockingBulkEnqueue) {
    SPSCQueue<int, 8> queue;  // Actual capacity: 7
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    std::vector<int> all_produced;
    std::vector<int> all_consumed;

    // Producer thread - will produce 20 items in batches
    std::thread producer([&queue, &producer_done, &all_produced]() {
        std::vector<int> batch1 = {1, 2, 3, 4, 5};
        std::vector<int> batch2 = {6, 7, 8, 9, 10};
        std::vector<int> batch3 = {11, 12, 13, 14, 15};
        std::vector<int> batch4 = {16, 17, 18, 19, 20};

        // Add all items to the produced vector
        all_produced.insert(all_produced.end(), batch1.begin(), batch1.end());
        all_produced.insert(all_produced.end(), batch2.begin(), batch2.end());
        all_produced.insert(all_produced.end(), batch3.begin(), batch3.end());
        all_produced.insert(all_produced.end(), batch4.begin(), batch4.end());

        // Enqueue batches with blocking behavior
        queue.enqueue_bulk(batch1.begin(), batch1.size());
        queue.enqueue_bulk(batch2.begin(), batch2.size());
        queue.enqueue_bulk(batch3.begin(), batch3.size());
        queue.enqueue_bulk(batch4.begin(), batch4.size());

        producer_done = true;
    });

    // Consumer thread - will consume all items
    std::thread consumer([&queue, &producer_done, &consumer_done, &all_consumed]() {
        std::vector<int> results(3);  // Small buffer to force multiple dequeues

        while (!producer_done || !queue.is_empty()) {
            size_t dequeued = queue.try_dequeue_bulk(results.begin(), results.size());
            if (dequeued > 0) {
                for (size_t i = 0; i < dequeued; i++) {
                    all_consumed.push_back(results[i]);
                }
            } else {
                std::this_thread::yield();  // Give producer time to produce
            }
        }

        consumer_done = true;
    });

    producer.join();
    consumer.join();

    // Verify all items were produced and consumed in order
    EXPECT_EQ(all_produced, all_consumed);
    EXPECT_EQ(all_consumed.size(), 20);
}

// Test timed bulk enqueue operations
TEST(SPSCQueueTest, TimedBulkEnqueue) {
    SPSCQueue<int, 4> queue;  // Actual capacity: 3

    // Fill the queue
    queue.try_enqueue(1);
    queue.try_enqueue(2);
    queue.try_enqueue(3);
    EXPECT_TRUE(queue.is_full());

    // Test timeout on full queue
    std::vector<int> items = {4, 5, 6};
    auto start = std::chrono::steady_clock::now();
    size_t enqueued = queue.enqueue_bulk_for(items.begin(), items.size(), std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(enqueued, 0);  // Should time out without enqueuing
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test successful timed enqueue after making space
    int item;
    queue.try_dequeue(item);  // Make space for one item
    EXPECT_EQ(item, 1);

    enqueued = queue.enqueue_bulk_for(items.begin(), items.size(), std::chrono::milliseconds(100));
    EXPECT_EQ(enqueued, 1);  // Should enqueue one item

    // Verify queue state
    std::vector<int> results(3);
    size_t dequeued = queue.try_dequeue_bulk(results.begin(), 3);
    EXPECT_EQ(dequeued, 3);
    EXPECT_EQ(results[0], 2);
    EXPECT_EQ(results[1], 3);
    EXPECT_EQ(results[2], 4);  // The first item from the timed bulk enqueue
}

// DYNAMIC QUEUE TESTS
// These tests verify the same functionality for the dynamic queue variant

// Basic tests for dynamic queue with non-trivial type
TEST(DynamicSPSCQueueTest, BasicOperations) {
    DynamicSPSCQueue<std::string> queue(16);

    // Test empty state
    EXPECT_TRUE(queue.is_empty());
    EXPECT_FALSE(queue.is_full());

    // Test enqueue
    EXPECT_TRUE(queue.try_enqueue("test1"));
    EXPECT_FALSE(queue.is_empty());

    // Test dequeue
    std::string item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test1");
    EXPECT_TRUE(queue.is_empty());

    // Test multiple items
    EXPECT_TRUE(queue.try_enqueue("test2"));
    EXPECT_TRUE(queue.try_enqueue("test3"));

    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test2");
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, "test3");
    EXPECT_TRUE(queue.is_empty());
}

// Test for dynamic queue with trivially copyable type
TEST(DynamicSPSCQueueTest, TrivialTypeOperations) {
    DynamicSPSCQueue<int> queue(16);

    EXPECT_TRUE(queue.try_enqueue(42));

    int item = 0;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, 42);
    EXPECT_TRUE(queue.is_empty());
}

// Test filling the dynamic queue to capacity
TEST(DynamicSPSCQueueTest, FullQueue) {
    DynamicSPSCQueue<int> queue(4);  // Actual capacity: 3

    EXPECT_TRUE(queue.try_enqueue(1));
    EXPECT_TRUE(queue.try_enqueue(2));
    EXPECT_TRUE(queue.try_enqueue(3));
    EXPECT_TRUE(queue.is_full());        // Should be full now
    EXPECT_FALSE(queue.try_enqueue(4));  // Should fail

    int item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item, 1);
    EXPECT_FALSE(queue.is_full());  // No longer full

    // Can enqueue again
    EXPECT_TRUE(queue.try_enqueue(4));
}

// Test wrap-around behavior with dynamic queue
TEST(DynamicSPSCQueueTest, WrapAround) {
    DynamicSPSCQueue<int> queue(4);  // Actual capacity: 3
    std::vector<int> results;

    // Fill and drain multiple times to force wrap-around
    for (int cycle = 0; cycle < 3; cycle++) {
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 1));
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 2));
        EXPECT_TRUE(queue.try_enqueue(cycle * 3 + 3));

        int item;
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
        EXPECT_TRUE(queue.try_dequeue(item));
        results.push_back(item);
    }

    // Verify correct order
    std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    EXPECT_EQ(results, expected);
}

// Test move semantics with dynamic queue
TEST(DynamicSPSCQueueTest, MoveSemantics) {
    DynamicSPSCQueue<std::unique_ptr<int>> queue(4);

    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(43);

    EXPECT_TRUE(queue.try_enqueue(std::move(ptr1)));
    EXPECT_TRUE(queue.try_enqueue(std::move(ptr2)));

    // Original pointers should be null after move
    EXPECT_EQ(ptr1, nullptr);
    EXPECT_EQ(ptr2, nullptr);

    std::unique_ptr<int> result;
    EXPECT_TRUE(queue.try_dequeue(result));
    EXPECT_EQ(*result, 42);

    EXPECT_TRUE(queue.try_dequeue(result));
    EXPECT_EQ(*result, 43);
}

// Test emplace functionality with dynamic queue
TEST(DynamicSPSCQueueTest, Emplace) {
    DynamicSPSCQueue<std::pair<int, std::string>> queue(4);

    EXPECT_TRUE(queue.try_emplace(1, "one"));
    EXPECT_TRUE(queue.try_emplace(2, "two"));

    std::pair<int, std::string> item;
    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item.first, 1);
    EXPECT_EQ(item.second, "one");

    EXPECT_TRUE(queue.try_dequeue(item));
    EXPECT_EQ(item.first, 2);
    EXPECT_EQ(item.second, "two");
}

// Test bulk timed operations with dynamic queue
TEST(DynamicSPSCQueueTest, BulkTimedOperations) {
    DynamicSPSCQueue<int> queue(16);

    // Test timeout on empty queue
    std::vector<int> results(5);
    auto start = std::chrono::steady_clock::now();
    size_t count = queue.dequeue_bulk_for(results.begin(), 5, std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(count, 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test with some items
    for (int i = 0; i < 3; i++) {
        queue.try_enqueue(i);
    }

    count = queue.dequeue_bulk_for(results.begin(), 5, std::chrono::milliseconds(100));
    EXPECT_EQ(count, 3);
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(results[i], static_cast<int>(i));
    }
}

// Test dequeue_bulk_for_any with dynamic queue
TEST(DynamicSPSCQueueTest, BulkTimedAnyOperations) {
    DynamicSPSCQueue<int> queue(16);

    // Test timeout on empty queue
    std::vector<int> results(5);
    auto start = std::chrono::steady_clock::now();
    size_t count = queue.dequeue_bulk_for_any(results.begin(), 5, std::chrono::milliseconds(100));
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(count, 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 90);  // Allow for some timing variation

    // Test with some items, added gradually
    std::thread producer([&queue]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.try_enqueue(42);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.try_enqueue(43);
        queue.try_enqueue(44);
    });

    results.assign(5, 0);
    count = queue.dequeue_bulk_for_any(results.begin(), 5, std::chrono::milliseconds(200));

    EXPECT_GE(count, 1);  // Should get at least the first item
    EXPECT_EQ(results[0], 42);

    if (count > 1) {
        EXPECT_EQ(results[1], 43);
    }

    producer.join();

    // Cleanup any remaining items
    queue.try_dequeue_bulk(results.begin(), 5);
}

// Test shutdown behavior with dynamic queue
TEST(DynamicSPSCQueueTest, Shutdown) {
    DynamicSPSCQueue<int> queue(4);
    std::atomic<bool> consumer_unblocked{false};

    // Start a consumer thread that will block
    std::thread consumer([&queue, &consumer_unblocked]() {
        int item;
        bool result = queue.dequeue(item);  // This should block
        EXPECT_FALSE(result);               // After shutdown, should return false
        consumer_unblocked = true;
    });

    // Give the consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Shutdown the queue
    queue.shutdown();

    // Consumer should unblock
    consumer.join();
    EXPECT_TRUE(consumer_unblocked);

    // After shutdown, operations should fail
    int item;
    EXPECT_FALSE(queue.dequeue(item));
    EXPECT_FALSE(queue.dequeue_for(item, std::chrono::milliseconds(1)));
}

// Test concurrent enqueue/dequeue with high throughput for dynamic queue
TEST(DynamicSPSCQueueTest, ConcurrentThroughput) {
    // Use fewer iterations for the dynamic test to reduce test time
    for (size_t runs = 0; runs < 50; ++runs) {
        constexpr size_t ITEM_COUNT = 100000;
        DynamicSPSCQueue<uint64_t> queue(1024);
        std::atomic<bool> error{false};
        std::atomic<bool> producer_done{false};

        std::thread producer([&queue, &error, &producer_done]() {
            try {
                for (uint64_t i = 0; i < ITEM_COUNT; i++) {
                    // Use non-blocking enqueue with retry
                    while (!queue.try_enqueue(i) && !error.load(std::memory_order_relaxed)) {
                        std::this_thread::yield();  // Give consumer time to catch up
                    }

                    // Exit early if consumer detected an error
                    if (error.load(std::memory_order_relaxed)) {
                        break;
                    }
                }
            } catch (...) { error = true; }
            producer_done.store(true, std::memory_order_release);
        });

        std::thread consumer([&queue, &error, &producer_done]() {
            try {
                uint64_t expected = 0;
                while (expected < ITEM_COUNT && !error.load(std::memory_order_relaxed)) {
                    uint64_t item;

                    // Use non-blocking dequeue with yield
                    if (queue.try_dequeue(item)) {
                        if (item != expected) {
                            error = true;
                            break;
                        }
                        expected++;
                    } else if (producer_done.load(std::memory_order_acquire) && queue.is_empty()) {
                        // Producer is done and the queue is drained, so any item not seen yet is missing
                        if (expected < ITEM_COUNT) {
                            error = true;
                        }
                        break;
                    } else {
                        std::this_thread::yield();  // Give producer time to produce
                    }
                }
            } catch (...) { error = true; }
        });

        // Set timeout for test to avoid hanging forever
        auto start_time = std::chrono::steady_clock::now();
        bool joined = false;

        // Try to join with timeout
        while (!joined && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
            producer.join();
            consumer.join();
            joined = true;
        }

        // If not joined within timeout, consider it a deadlock
        EXPECT_TRUE(joined) << "Test timed out - likely deadlock";
        EXPECT_FALSE(error) << "Data corruption or missing items detected";
    }
}

// REGRESSION TESTS
// Each of these covers a defect of the previous queue implementation

namespace {
    using namespace std::chrono_literals;

    // Not default-constructible, owns heap memory (so ASAN sees leaks) and counts live objects (double destruction)
    struct Tracked {
        static inline int live = 0;
        static inline int copies_until_throw = -1;  // < 0: never throw

        std::string payload;

        explicit Tracked(int value) : payload("tracked-value-with-heap-storage-" + std::to_string(value)) { ++live; }
        Tracked(const Tracked& other) : payload(other.payload) {
            if (copies_until_throw == 0) {
                throw std::runtime_error("copy failed");
            }
            if (copies_until_throw > 0) {
                --copies_until_throw;
            }
            ++live;
        }
        Tracked(Tracked&& other) noexcept : payload(std::move(other.payload)) { ++live; }
        Tracked& operator=(const Tracked&) = default;
        Tracked& operator=(Tracked&&) noexcept = default;
        ~Tracked() { --live; }
    };

    template<typename Queue, typename... QueueArgs>
    void checkElementLifetimes(QueueArgs... queueArgs) {
        const int before = Tracked::live;
        {
            auto queue = std::make_unique<Queue>(queueArgs...);  // capacity 15
            Tracked out(-1);
            // 50 enqueues through 16 slots: wraps around several times
            for (int round = 0; round < 10; ++round) {
                for (int i = 0; i < 5; ++i) {
                    EXPECT_TRUE(queue->try_emplace(round * 5 + i));
                }
                for (int i = 0; i < 4; ++i) {
                    EXPECT_TRUE(queue->try_dequeue(out));
                }
            }
            EXPECT_EQ(queue->size(), 10u);
            EXPECT_EQ(Tracked::live, before + 1 + 10);

            std::vector<Tracked> src;
            for (int i = 0; i < 5; ++i) {
                src.emplace_back(100 + i);
            }
            EXPECT_EQ(queue->try_enqueue_bulk(src.begin(), src.size()), 5u);
            EXPECT_TRUE(queue->is_full());

            std::vector<Tracked> dst(6, Tracked(0));
            EXPECT_EQ(queue->try_dequeue_bulk(dst.begin(), dst.size()), 6u);
            EXPECT_EQ(queue->clear(), 9u);
            EXPECT_TRUE(queue->is_empty());
            EXPECT_EQ(Tracked::live, before + 1 + 5 + 6);

            // Left in the queue for the destructor
            EXPECT_TRUE(queue->try_emplace(7));
            EXPECT_TRUE(queue->try_emplace(8));
        }
        EXPECT_EQ(Tracked::live, before);
    }

    // Lets the other side repeatedly go from spinning to sleeping, to exercise the sleep/wake handshake
    void randomPause(std::mt19937& rng) {
        if (rng() % 32 == 0) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(rng() % 100);
            while (std::chrono::steady_clock::now() < until) {}
        }
    }

    // Runs body; if it does not finish within the deadline, shuts the queue down so the test fails instead of hanging
    template<typename Queue, typename Body>
    void withWatchdog(Queue& queue, Body body) {
        std::atomic<bool> done{false};
        std::thread watchdog([&] {
            const auto deadline = std::chrono::steady_clock::now() + 60s;
            while (!done.load() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(10ms);
            }
            if (!done.load()) {
                ADD_FAILURE() << "Blocking hand-off hung (lost wake-up)";
                queue.shutdown();
            }
        });
        body();
        done = true;
        watchdog.join();
    }

    template<typename Queue>
    void checkBlockingHandOff(Queue& queue) {
        constexpr int kItems = 200000;
        withWatchdog(queue, [&] {
            std::thread producer([&] {
                std::mt19937 rng(1);
                for (int i = 0; i < kItems; ++i) {
                    if (!queue.enqueue(i)) {
                        return;
                    }
                    randomPause(rng);
                }
            });
            std::mt19937 rng(2);
            int item = -1;
            int received = 0;
            for (; received < kItems; ++received) {
                if (!queue.dequeue(item) || item != received) {
                    break;
                }
                randomPause(rng);
            }
            producer.join();
            EXPECT_EQ(received, kItems);
        });
    }

    template<typename Queue>
    void checkBlockingBulkHandOff(Queue& queue) {
        constexpr int kItems = 300000;
        std::vector<int> produced(kItems);
        std::iota(produced.begin(), produced.end(), 0);
        std::vector<int> consumed(kItems, -1);
        withWatchdog(queue, [&] {
            std::thread producer([&] {
                std::mt19937 rng(3);
                for (std::size_t pos = 0, chunk = 1; pos < produced.size(); pos += chunk, chunk = chunk % 37 + 1) {
                    chunk = std::min(chunk, produced.size() - pos);
                    if (queue.enqueue_bulk(produced.data() + pos, chunk) != chunk) {
                        return;
                    }
                    randomPause(rng);
                }
            });
            std::mt19937 rng(4);
            for (std::size_t pos = 0, chunk = 1; pos < consumed.size(); pos += chunk, chunk = chunk % 23 + 1) {
                chunk = std::min(chunk, consumed.size() - pos);
                if (queue.dequeue_bulk(consumed.begin() + static_cast<std::ptrdiff_t>(pos), chunk) != chunk) {
                    break;
                }
                randomPause(rng);
            }
            producer.join();
        });
        EXPECT_EQ(consumed, produced);
    }
}  // namespace

// The static queue used to construct elements on top of live std::array elements and leak or double-destroy them;
// try_dequeue skipped the destructor of moved-from std::string, std::vector<bool>, pointers and containers
TEST(SPSCQueueRegression, ElementLifetimesStatic) { checkElementLifetimes<SPSCQueue<Tracked, 16>>(); }

TEST(SPSCQueueRegression, ElementLifetimesDynamic) { checkElementLifetimes<DynamicSPSCQueue<Tracked>>(16); }

// A throwing copy in the middle of a wrapped bulk enqueue must not leak the elements already constructed
TEST(SPSCQueueRegression, BulkEnqueueIsExceptionSafe) {
    const int before = Tracked::live;
    {
        SPSCQueue<Tracked, 8> queue;  // capacity 7, 8 slots
        Tracked out(0);
        for (int i = 0; i < 6; ++i) {
            ASSERT_TRUE(queue.try_emplace(i));
            ASSERT_TRUE(queue.try_dequeue(out));
        }
        std::vector<Tracked> src;
        for (int i = 0; i < 5; ++i) {
            src.emplace_back(i);
        }
        Tracked::copies_until_throw = 3;  // fails in the second chunk, after wrapping around
        EXPECT_THROW(queue.try_enqueue_bulk(src.begin(), src.size()), std::runtime_error);
        Tracked::copies_until_throw = -1;
        EXPECT_TRUE(queue.is_empty());
        EXPECT_EQ(Tracked::live, before + 1 + 5);
        EXPECT_EQ(queue.try_enqueue_bulk(src.begin(), src.size()), 5u);
    }
    EXPECT_EQ(Tracked::live, before);
}

// dequeue_bulk returned 0 on stop although it had already moved elements out of the queue
TEST(SPSCQueueRegression, BulkDequeueReportsItemsTakenBeforeStop) {
    DynamicSPSCQueue<int> queue(64);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue.try_enqueue(i));
    }
    std::stop_source stop;
    std::thread stopper([&] {
        std::this_thread::sleep_for(50ms);
        stop.request_stop();
    });
    std::vector<int> out(10, -1);
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(queue.dequeue_bulk(out.data(), out.size(), stop.get_token()), 5u);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);  // woken by the stop request, not by polling
    stopper.join();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(out[static_cast<std::size_t>(i)], i);
    }
    EXPECT_TRUE(queue.is_empty());
}

// Shutdown must wake a blocked bulk dequeue and report the partial count
TEST(SPSCQueueRegression, ShutdownWakesBlockedBulkDequeue) {
    DynamicSPSCQueue<int> queue(64);
    ASSERT_TRUE(queue.try_enqueue(1));
    std::thread closer([&] {
        std::this_thread::sleep_for(50ms);
        queue.shutdown();
    });
    std::vector<int> out(4, -1);
    EXPECT_EQ(queue.dequeue_bulk(out.begin(), out.size()), 1u);
    closer.join();
    EXPECT_EQ(out[0], 1);
}

// The stop-token overloads waited on a temporary condition variable that was never notified, so they only returned
// on stop; the timed variant did not compile and reported failure after taking the last element
TEST(SPSCQueueRegression, StopTokenOverloadsWakeOnData) {
    SPSCQueue<int, 16> queue;
    std::stop_source stop;  // never triggered

    std::thread producer([&] {
        std::this_thread::sleep_for(50ms);
        queue.try_enqueue(7);
    });
    int item = 0;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(queue.dequeue(item, stop.get_token()));
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    EXPECT_EQ(item, 7);
    producer.join();

    std::thread producer2([&] {
        std::this_thread::sleep_for(20ms);
        queue.try_enqueue(8);
    });
    EXPECT_TRUE(queue.dequeue_for(item, 1s, stop.get_token()));
    EXPECT_EQ(item, 8);
    EXPECT_TRUE(queue.is_empty());
    producer2.join();
}

// enqueue(T&&, stop_token) ignored the stop token
TEST(SPSCQueueRegression, BlockingEnqueueHonoursStopToken) {
    SPSCQueue<int, 4> queue;  // capacity 3
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(queue.try_enqueue(i));
    }
    std::stop_source stop;
    std::thread stopper([&] {
        std::this_thread::sleep_for(50ms);
        stop.request_stop();
    });
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(queue.enqueue(3, stop.get_token()));
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    stopper.join();
    EXPECT_EQ(queue.size(), 3u);

    std::thread consumer([&] {
        std::this_thread::sleep_for(20ms);
        int item = 0;
        queue.try_dequeue(item);
    });
    EXPECT_TRUE(queue.enqueue(3));  // woken by the consumer freeing a slot
    consumer.join();
}

// dequeue_for only compiled for millisecond durations
TEST(SPSCQueueRegression, TimedOperationsAcceptAnyDuration) {
    DynamicSPSCQueue<int> queue(8);
    int item = 0;
    EXPECT_FALSE(queue.dequeue_for(item, std::chrono::microseconds(500)));
    EXPECT_FALSE(queue.dequeue_for(item, std::chrono::duration<double>(0.001)));
    EXPECT_FALSE(queue.dequeue_for(item, std::chrono::nanoseconds(0)));
    ASSERT_TRUE(queue.try_enqueue(5));
    EXPECT_TRUE(queue.dequeue_for(item, std::chrono::hours(1)));
    EXPECT_EQ(item, 5);
}

// Notifications without the mutex could be lost, leaving blocking operations asleep with work available
TEST(SPSCQueueRegression, BlockingHandOffNeverLosesWakeups) {
    SPSCQueue<int, 4> queue;
    checkBlockingHandOff(queue);
}

TEST(SPSCQueueRegression, BlockingHandOffNeverLosesWakeupsDynamic) {
    DynamicSPSCQueue<int> queue(4);
    checkBlockingHandOff(queue);
}

TEST(SPSCQueueRegression, BlockingBulkHandOff) {
    DynamicSPSCQueue<int> queue(16);
    checkBlockingBulkHandOff(queue);
}

// The driver's paths: const pointer in (store), vector iterator out (getData) and raw pointer out (loadMap)
TEST(SPSCQueueRegression, BulkCopyPathsAcrossWrapAround) {
    DynamicSPSCQueue<std::uint8_t> queue(16);  // capacity 15
    std::uint8_t next = 0;
    for (std::size_t round = 0; round < 100; ++round) {
        const std::size_t n = 1 + round % 15;
        std::vector<std::uint8_t> chunk(n);
        for (auto& value : chunk) {
            value = next++;
        }
        const std::uint8_t* src = chunk.data();
        ASSERT_EQ(queue.try_enqueue_bulk(src, n), n);
        std::vector<std::uint8_t> out(n);
        if (round % 2 == 0) {
            ASSERT_EQ(queue.try_dequeue_bulk(out.begin(), n), n);
        } else {
            ASSERT_EQ(queue.try_dequeue_bulk(out.data(), n), n);
        }
        EXPECT_EQ(out, chunk);
    }

    // Non-contiguous iterators take the element-wise path
    DynamicSPSCQueue<int> intQueue(8);
    std::deque<int> source = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    EXPECT_EQ(intQueue.try_enqueue_bulk(source.begin(), source.size()), 7u);
    std::list<int> sink;
    EXPECT_EQ(intQueue.try_dequeue_bulk(std::back_inserter(sink), 10), 7u);
    EXPECT_EQ(sink, (std::list<int>{1, 2, 3, 4, 5, 6, 7}));
}

// After shutdown, blocking calls stop waiting but queued elements can still be drained
TEST(SPSCQueueRegression, ShutdownStillDrainsRemainingItems) {
    SPSCQueue<int, 8> queue;
    ASSERT_TRUE(queue.try_enqueue(1));
    ASSERT_TRUE(queue.try_enqueue(2));
    queue.shutdown();
    EXPECT_FALSE(queue.is_active());

    int item = 0;
    EXPECT_TRUE(queue.dequeue(item));
    EXPECT_EQ(item, 1);
    std::vector<int> out(4, -1);
    EXPECT_EQ(queue.dequeue_bulk(out.begin(), out.size()), 1u);
    EXPECT_EQ(out[0], 2);
    EXPECT_FALSE(queue.dequeue(item));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}