#include "core/concurrent/lock_free_queue.h"
#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <numeric>
#include <algorithm>

// [SEQUENCE: MVP6-45] Unit tests for the LockFreeQueue.
TEST(LockFreeQueueTest, SingleThreadedEnqueueDequeue) {
    mmorpg::concurrent::LockFreeQueue<int> q;
    int val;

    EXPECT_FALSE(q.Dequeue(val));

    q.Enqueue(10);
    ASSERT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 10);
    EXPECT_FALSE(q.Dequeue(val));

    q.Enqueue(20);
    q.Enqueue(30);
    ASSERT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 20);
    ASSERT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 30);
    EXPECT_FALSE(q.Dequeue(val));
}

TEST(LockFreeQueueTest, MultiProducerSingleConsumer) {
    mmorpg::concurrent::LockFreeQueue<int> q;
    std::vector<std::thread> producers;
    const int num_producers = 4;
    const int items_per_producer = 1000;

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&q, i, items_per_producer]() {
            for (int j = 0; j < items_per_producer; ++j) {
                q.Enqueue(i * items_per_producer + j);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    std::vector<int> dequeued_items;
    int val;
    while (q.Dequeue(val)) {
        dequeued_items.push_back(val);
    }

    EXPECT_EQ(dequeued_items.size(), num_producers * items_per_producer);
    
    // Verify that all items are present, even if the order is not guaranteed
    // between producers.
    std::sort(dequeued_items.begin(), dequeued_items.end());
    std::vector<int> expected_items(num_producers * items_per_producer);
    std::iota(expected_items.begin(), expected_items.end(), 0);

    EXPECT_EQ(dequeued_items, expected_items);
}
