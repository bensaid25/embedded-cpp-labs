#pragma once

#include <limits>
#include <type_traits>

namespace ecl::detail {

/// True if x is a finite floating-point value (not NaN, not +/- infinity).
///
/// NaN fails both comparisons, and the infinities lie outside [-max, max], so
/// only finite values pass. It works in constexpr code (std::isfinite is not
/// constexpr in C++17). Like every NaN test it is unreliable under
/// -ffast-math or -ffinite-math-only, which let the compiler assume that NaN
/// and infinity never occur.
template <typename T>
[[nodiscard]] constexpr bool is_finite(T x) noexcept {
    static_assert(std::is_floating_point_v<T>,
                  "is_finite requires a floating-point type");
    return x >= -std::numeric_limits<T>::max() &&
           x <= std::numeric_limits<T>::max();
}

}  // namespace ecl::detail
