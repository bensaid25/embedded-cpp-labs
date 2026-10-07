#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ecl {

/// Lock-free, wait-free queue for exactly ONE producer and ONE consumer.
///
/// Typical use: an interrupt handler (or a sampling thread) produces readings
/// and the main loop consumes them, with no mutex and no interrupt masking.
///
/// Guarantees:
///  - No heap allocation: storage is a std::array<T, Capacity>.
///  - try_push() and try_pop() never block, never loop and never throw, so
///    they are safe to call from an interrupt handler.
///  - Items come out in the order they went in. A full queue rejects the new
///    item (it never overwrites), and each rejection is counted in dropped().
///
/// Rules (not checked at run time):
///  - Only one context may call try_push().
///  - Only one context may call try_pop().
///  - Any context may call capacity(), size_approx(), empty_approx() and
///    dropped(). Size and emptiness are only snapshots, because the other
///    side may change the queue right after you look.
///
/// Requirements on T: trivially copyable and default constructible. The
/// first keeps a copy short and cheap inside an interrupt handler.
///
/// Capacity must be a power of two, so an index wraps with a bit mask
/// instead of a division (many small cores have no hardware divider).
///
/// Memory ordering: the producer writes the slot first, then publishes it
/// with a release store of head_. The consumer reads head_ with acquire, so
/// it is guaranteed to see the finished slot. The consumer frees a slot with
/// a release store of tail_, which the producer reads with acquire before it
/// reuses that slot. Each index is written by exactly one side, so no
/// read-modify-write operation is needed.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "SpscQueue capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscQueue<T> requires a trivially copyable T");
    static_assert(std::is_default_constructible_v<T>,
                  "SpscQueue<T> requires a default-constructible T");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "SpscQueue needs a lock-free atomic<size_t> on this target");

public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] static constexpr size_type capacity() noexcept {
        return Capacity;
    }

    /// PRODUCER ONLY. Adds a value if there is room. Returns false, counts a
    /// drop and changes nothing when the queue is full.
    [[nodiscard]] bool try_push(const T& value) noexcept {
        const size_type head = head_.load(std::memory_order_relaxed);
        const size_type tail = tail_.load(std::memory_order_acquire);
        if (head - tail == Capacity) {
            // Only the producer writes dropped_, so a plain load + store is
            // enough (and works even where atomic read-modify-write is not).
            dropped_.store(dropped_.load(std::memory_order_relaxed) + 1,
                           std::memory_order_relaxed);
            return false;
        }
        data_[head & kMask] = value;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    /// CONSUMER ONLY. Removes and returns the oldest value, or std::nullopt
    /// if the queue is empty.
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const size_type tail = tail_.load(std::memory_order_relaxed);
        const size_type head = head_.load(std::memory_order_acquire);
        if (head == tail) {
            return std::nullopt;
        }
        const T value = data_[tail & kMask];
        tail_.store(tail + 1, std::memory_order_release);
        return value;
    }

    /// Snapshot of the number of stored values.
    [[nodiscard]] size_type size_approx() const noexcept {
        // Read tail first: head can only move forward, so the difference can
        // never come out negative.
        const size_type tail = tail_.load(std::memory_order_acquire);
        const size_type head = head_.load(std::memory_order_acquire);
        return head - tail;
    }

    /// Snapshot: true if the queue held no values a moment ago.
    [[nodiscard]] bool empty_approx() const noexcept {
        return size_approx() == 0;
    }

    /// Number of values rejected because the queue was full.
    [[nodiscard]] size_type dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    static constexpr size_type kMask = Capacity - 1;

    std::array<T, Capacity> data_{};
    // Free-running counters: they only ever increase and wrap naturally.
    // Capacity divides the counter range, so (head - tail) stays correct
    // across the wrap.
    std::atomic<size_type> head_{0};  // next slot to write (producer owns)
    std::atomic<size_type> tail_{0};  // next slot to read  (consumer owns)
    std::atomic<size_type> dropped_{0};
};

}  // namespace ecl
