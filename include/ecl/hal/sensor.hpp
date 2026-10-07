#pragma once

#include <type_traits>
#include <utility>

#include <ecl/sensor_reading.hpp>

namespace ecl::hal {

/// The sensor contract, with no virtual functions and no base class.
///
/// A sensor is any type with a member function
///
///     ecl::SensorReading read(ecl::SensorReading::Timestamp now_ms) noexcept;
///
/// The caller owns the clock and passes the time in, so the same pipeline
/// code runs on the host with a simulated clock and on a microcontroller
/// with a hardware timer. A failed measurement is reported by returning an
/// invalid SensorReading, never by throwing.
///
/// Real drivers (an I2C accelerometer on an ESP32, say) implement the same
/// function; the pipeline cannot tell the difference. Because the type is a
/// template parameter, the call is resolved at compile time: no vtable, no
/// indirect call, and it can be inlined.
template <typename S, typename = void>
struct is_sensor : std::false_type {};

template <typename S>
struct is_sensor<
    S, std::enable_if_t<std::is_same_v<
           decltype(std::declval<S&>().read(
               std::declval<SensorReading::Timestamp>())),
           SensorReading>>> : std::true_type {};

template <typename S>
inline constexpr bool is_sensor_v = is_sensor<S>::value;

}  // namespace ecl::hal
