#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ecl {

/// Fixed-capacity circular buffer (FIFO). Oldest element is index 0.
///
/// Guarantees:
///  - No heap allocation: storage is a std::array<T, Capacity>.
///  - O(1) push, try_push, pop, front, back, operator[].
///  - Memory use is Capacity * sizeof(T) plus two size_t counters.
///
/// Requirements on T: default constructible and copy assignable.
///
/// Preconditions (checked with assert in debug builds):
///  - front(), back() require !empty().
///  - operator[](i) requires i < size().
template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "RingBuffer capacity must be at least 1");
    static_assert(std::is_default_constructible_v<T>,
                  "RingBuffer<T> requires a default-constructible T");

public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] static constexpr size_type capacity() noexcept {
        return Capacity;
    }
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr bool full() const noexcept {
        return size_ == Capacity;
    }

    /// Adds a value. If the buffer is full, the oldest value is overwritten.
    void push(const T& value) {
        if (full()) {
            // The slot after the newest element is the oldest element.
            data_[head_] = value;
            head_ = (head_ + 1) % Capacity;
        } else {
            data_[(head_ + size_) % Capacity] = value;
            ++size_;
        }
    }

    /// Adds a value only if there is room. Returns false (and changes
    /// nothing) when the buffer is full.
    [[nodiscard]] bool try_push(const T& value) {
        if (full()) {
            return false;
        }
        push(value);
        return true;
    }

    /// Removes and returns the oldest value, or std::nullopt if empty.
    [[nodiscard]] std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        T value = data_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        return value;
    }

    /// Oldest value. Precondition: !empty().
    [[nodiscard]] const T& front() const noexcept {
        assert(!empty());
        return data_[head_];
    }

    /// Newest value. Precondition: !empty().
    [[nodiscard]] const T& back() const noexcept {
        assert(!empty());
        return data_[(head_ + size_ - 1) % Capacity];
    }

    /// i-th value counting from the oldest. Precondition: i < size().
    [[nodiscard]] const T& operator[](size_type i) const noexcept {
        assert(i < size_);
        return data_[(head_ + i) % Capacity];
    }

    /// Removes all values.
    void clear() noexcept {
        head_ = 0;
        size_ = 0;
    }

private:
    std::array<T, Capacity> data_{};
    size_type head_{0};  // index of the oldest element
    size_type size_{0};  // number of stored elements
};

}  // namespace ecl
