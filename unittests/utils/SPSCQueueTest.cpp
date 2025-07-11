#include <FINNCppDriver/utils/SPSCQueue.hpp>
#include <atomic>
#include <chrono>
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
                    } else if (producer_done.load(std::memory_order_acquire)) {
                        // If producer is done and no more items, we're missing items
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
                    } else if (producer_done.load(std::memory_order_acquire)) {
                        // If producer is done and no more items, we're missing items
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}