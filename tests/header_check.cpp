// Compiled with -fno-exceptions -fno-rtti and strict warnings (see CMakeLists).
// Explicit instantiation forces the compiler to check EVERY member function of
// each template, not only the ones the tests happen to call.

#include <ecl/moving_average.hpp>
#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/statistics.hpp>
#include <ecl/threshold_detector.hpp>
#include <vibration_pipeline.hpp>

template class ecl::RingBuffer<float, 8>;
template class ecl::RingBuffer<ecl::SensorReading, 4>;
template class ecl::Statistics<float, double>;
template class ecl::Statistics<float, float>;
template class ecl::Statistics<double, double>;
template class ecl::MovingAverage<8, float, double>;
template class ecl::MovingAverage<4, float, float>;
template class ecl::MovingAverage<2, double, double>;
template class ecl::ThresholdDetector<float>;
template class ecl::ThresholdDetector<double>;
template class demo::VibrationPipeline<200, 32, 8>;
