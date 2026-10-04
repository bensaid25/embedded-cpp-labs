#pragma once

#include <type_traits>

namespace ecl::detail {

/// True if x is a finite floating-point value (not NaN, not +/- infinity).
///
/// x - x is 0 for every finite x and NaN otherwise, so this works in
/// constexpr code without <cmath> (std::isfinite is not constexpr in C++17).
/// Note: it relies on IEEE semantics, so it is not reliable under
/// -ffast-math, which lets the compiler assume NaN and infinity never occur.
template <typename T>
[[nodiscard]] constexpr bool is_finite(T x) noexcept {
    static_assert(std::is_floating_point_v<T>,
                  "is_finite requires a floating-point type");
    return x - x == T{0};
}

}  // namespace ecl::detail
