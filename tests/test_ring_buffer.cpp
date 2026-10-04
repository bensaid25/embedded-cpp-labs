#include <gtest/gtest.h>

#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>

using ecl::RingBuffer;
using ecl::SensorReading;

static_assert(RingBuffer<int, 4>::capacity() == 4);

TEST(RingBuffer, NewBufferIsEmpty) {
    RingBuffer<int, 3> buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_FALSE(buf.full());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.capacity(), 3u);
}

TEST(RingBuffer, PushIncreasesSize) {
    RingBuffer<int, 3> buf;
    buf.push(10);
    EXPECT_EQ(buf.size(), 1u);
    EXPECT_FALSE(buf.empty());
    EXPECT_EQ(buf.front(), 10);
    EXPECT_EQ(buf.back(), 10);
}

TEST(RingBuffer, BecomesFullAtCapacity) {
    RingBuffer<int, 3> buf;
    buf.push(1);
    buf.push(2);
    EXPECT_FALSE(buf.full());
    buf.push(3);
    EXPECT_TRUE(buf.full());
    EXPECT_EQ(buf.size(), 3u);
}

TEST(RingBuffer, PopReturnsOldestFirst) {
    RingBuffer<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    EXPECT_EQ(buf.pop(), 1);
    EXPECT_EQ(buf.pop(), 2);
    EXPECT_EQ(buf.pop(), 3);
    EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, PopFromEmptyReturnsNullopt) {
    RingBuffer<int, 3> buf;
    EXPECT_EQ(buf.pop(), std::nullopt);
    EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, PushWhenFullOverwritesOldest) {
    RingBuffer<int, 3> buf;
    for (int i = 1; i <= 4; ++i) {
        buf.push(i);
    }
    EXPECT_TRUE(buf.full());
    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf.front(), 2);  // 1 was overwritten
    EXPECT_EQ(buf.back(), 4);
}

TEST(RingBuffer, PopAfterOverwriteKeepsOrder) {
    RingBuffer<int, 3> buf;
    for (int i = 1; i <= 4; ++i) {
        buf.push(i);
    }
    EXPECT_EQ(buf.pop(), 2);
    EXPECT_EQ(buf.pop(), 3);
    EXPECT_EQ(buf.pop(), 4);
    EXPECT_EQ(buf.pop(), std::nullopt);
}

TEST(RingBuffer, TryPushWhenFullRejectsAndKeepsData) {
    RingBuffer<int, 2> buf;
    EXPECT_TRUE(buf.try_push(1));
    EXPECT_TRUE(buf.try_push(2));
    EXPECT_FALSE(buf.try_push(3));
    EXPECT_EQ(buf.size(), 2u);
    EXPECT_EQ(buf.front(), 1);
    EXPECT_EQ(buf.back(), 2);
}

TEST(RingBuffer, WrapAroundKeepsFifoOrder) {
    RingBuffer<int, 3> buf;
    int next_in = 0;
    int next_out = 0;
    // Push two, pop two: the head index travels around the storage
    // several times, crossing the end of the array.
    for (int round = 0; round < 10; ++round) {
        buf.push(next_in++);
        buf.push(next_in++);
        EXPECT_EQ(buf.pop(), next_out++);
        EXPECT_EQ(buf.pop(), next_out++);
    }
    EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, CapacityOneBehavesCorrectly) {
    RingBuffer<int, 1> buf;
    buf.push(1);
    EXPECT_TRUE(buf.full());
    buf.push(2);  // overwrites
    EXPECT_EQ(buf.size(), 1u);
    EXPECT_EQ(buf.front(), 2);
    EXPECT_EQ(buf.back(), 2);
    EXPECT_FALSE(buf.try_push(3));
    EXPECT_EQ(buf.pop(), 2);
    EXPECT_TRUE(buf.empty());
}

TEST(RingBuffer, IndexOperatorCountsFromOldestAfterWrap) {
    RingBuffer<int, 3> buf;
    for (int i = 1; i <= 5; ++i) {
        buf.push(i);  // holds 3, 4, 5
    }
    EXPECT_EQ(buf[0], 3);
    EXPECT_EQ(buf[1], 4);
    EXPECT_EQ(buf[2], 5);
}

TEST(RingBuffer, ClearEmptiesAndBufferIsReusable) {
    RingBuffer<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.pop(), std::nullopt);
    buf.push(7);
    EXPECT_EQ(buf.front(), 7);
    EXPECT_EQ(buf.size(), 1u);
}

TEST(RingBuffer, StoresSensorReadings) {
    RingBuffer<SensorReading, 4> buf;
    buf.push(SensorReading{20.0f, 100});
    buf.push(SensorReading{21.0f, 200});
    buf.push(SensorReading::invalid(300));

    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf.pop(), (SensorReading{20.0f, 100}));
    EXPECT_EQ(buf.pop(), (SensorReading{21.0f, 200}));
    auto last = buf.pop();
    ASSERT_TRUE(last.has_value());
    EXPECT_FALSE(last->is_valid());
}
