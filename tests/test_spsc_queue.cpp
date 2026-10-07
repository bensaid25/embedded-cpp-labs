#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>

#include <ecl/sensor_reading.hpp>
#include <ecl/spsc_queue.hpp>

using ecl::SensorReading;
using ecl::SpscQueue;

TEST(SpscQueue, StartsEmpty) {
    SpscQueue<int, 4> q;
    EXPECT_TRUE(q.empty_approx());
    EXPECT_EQ(q.size_approx(), 0u);
    EXPECT_EQ(q.dropped(), 0u);
    EXPECT_FALSE(q.try_pop().has_value());
    EXPECT_EQ(q.capacity(), 4u);
}

TEST(SpscQueue, KeepsFirstInFirstOutOrder) {
    SpscQueue<int, 8> q;
    for (int i = 1; i <= 5; ++i) {
        ASSERT_TRUE(q.try_push(i));
    }
    EXPECT_EQ(q.size_approx(), 5u);
    for (int i = 1; i <= 5; ++i) {
        const auto v = q.try_pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
    EXPECT_TRUE(q.empty_approx());
}

TEST(SpscQueue, UsesTheWholeCapacity) {
    SpscQueue<int, 4> q;
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }
    EXPECT_EQ(q.size_approx(), 4u);
}

TEST(SpscQueue, FullQueueRejectsAndCountsDrops) {
    SpscQueue<int, 2> q;
    ASSERT_TRUE(q.try_push(10));
    ASSERT_TRUE(q.try_push(20));
    EXPECT_FALSE(q.try_push(30));
    EXPECT_FALSE(q.try_push(40));
    EXPECT_EQ(q.dropped(), 2u);
    // The stored values are untouched: nothing was overwritten.
    EXPECT_EQ(*q.try_pop(), 10);
    EXPECT_EQ(*q.try_pop(), 20);
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(SpscQueue, AcceptsAgainAfterBeingDrained) {
    SpscQueue<int, 2> q;
    ASSERT_TRUE(q.try_push(1));
    ASSERT_TRUE(q.try_push(2));
    ASSERT_FALSE(q.try_push(3));
    ASSERT_EQ(*q.try_pop(), 1);
    EXPECT_TRUE(q.try_push(4));
    EXPECT_EQ(*q.try_pop(), 2);
    EXPECT_EQ(*q.try_pop(), 4);
}

TEST(SpscQueue, CapacityOfOneWorks) {
    SpscQueue<int, 1> q;
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(q.try_push(i));
        ASSERT_FALSE(q.try_push(-1));
        ASSERT_EQ(*q.try_pop(), i);
    }
    EXPECT_EQ(q.dropped(), 10u);
}

TEST(SpscQueue, SurvivesManyWrapArounds) {
    SpscQueue<std::uint32_t, 4> q;
    std::uint32_t next_in = 0;
    std::uint32_t next_out = 0;
    // Push 3, pop 3, over and over: the indices wrap the array many times.
    for (int round = 0; round < 5000; ++round) {
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(q.try_push(next_in++));
        }
        for (int i = 0; i < 3; ++i) {
            const auto v = q.try_pop();
            ASSERT_TRUE(v.has_value());
            ASSERT_EQ(*v, next_out++);
        }
    }
    EXPECT_TRUE(q.empty_approx());
    EXPECT_EQ(q.dropped(), 0u);
}

TEST(SpscQueue, CarriesSensorReadingsIntact) {
    SpscQueue<SensorReading, 4> q;
    ASSERT_TRUE(q.try_push(SensorReading{21.5f, 100}));
    ASSERT_TRUE(q.try_push(SensorReading::invalid(200)));
    EXPECT_EQ(*q.try_pop(), (SensorReading{21.5f, 100}));
    EXPECT_EQ(*q.try_pop(), SensorReading::invalid(200));
}

// The point of the whole class: one thread produces, another consumes, with
// no lock. Run under ThreadSanitizer in CI, this also proves the memory
// ordering is right (a wrong ordering is reported as a data race).
TEST(SpscQueue, TwoThreadsKeepEveryItemInOrder) {
    constexpr std::uint32_t kItems = 200000;
    SpscQueue<SensorReading, 64> q;

    std::thread producer([&q] {
        for (std::uint32_t i = 0; i < kItems; ++i) {
            // value and timestamp carry the same number, so a torn or stale
            // read shows up as a mismatch.
            const SensorReading r{static_cast<float>(i), i};
            while (!q.try_push(r)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint32_t expected = 0;
    bool ok = true;
    while (expected < kItems) {
        const auto r = q.try_pop();
        if (!r) {
            std::this_thread::yield();
            continue;
        }
        if (r->timestamp_ms() != expected ||
            r->value() != static_cast<float>(expected) || !r->is_valid()) {
            ok = false;
            break;
        }
        ++expected;
    }
    // On a failure above the producer may still be waiting for room, so
    // drain it before join to avoid hanging the test.
    while (!ok && q.try_pop()) {
    }
    producer.join();

    EXPECT_TRUE(ok) << "mismatch at item " << expected;
    EXPECT_EQ(expected, kItems);
}

TEST(SpscQueue, TwoThreadsWithDropsNeverReorderOrLose) {
    // The producer never waits (like an interrupt handler). Items that do not
    // fit are dropped. What arrives must be in order, and
    // received + dropped must equal sent.
    constexpr std::uint32_t kItems = 100000;
    SpscQueue<std::uint32_t, 16> q;
    std::atomic<bool> producer_done{false};

    std::thread producer([&] {
        for (std::uint32_t i = 0; i < kItems; ++i) {
            (void)q.try_push(i);
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::uint32_t received = 0;
    bool first = true;
    std::uint32_t previous = 0;
    bool ordered = true;
    for (;;) {
        // Read the flag BEFORE draining: if it was already set, one more
        // drain pass is guaranteed to see everything the producer pushed.
        const bool done = producer_done.load(std::memory_order_acquire);
        while (const auto v = q.try_pop()) {
            if (!first && *v <= previous) {
                ordered = false;
            }
            first = false;
            previous = *v;
            ++received;
        }
        if (done) {
            break;
        }
        std::this_thread::yield();
    }
    producer.join();

    EXPECT_TRUE(ordered);
    EXPECT_EQ(received + q.dropped(), kItems);
}

static_assert(std::is_trivially_copyable_v<SensorReading>);
