include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)  # the SpscQueue stress tests use std::thread

add_executable(ecl_tests
    test_sanity.cpp
    test_sensor_reading.cpp
    test_ring_buffer.cpp
    test_statistics.cpp
    test_moving_average.cpp
    test_threshold_detector.cpp
    test_vibration_pipeline.cpp
    test_spsc_queue.cpp
    test_mock_sensor.cpp
    test_sampler.cpp
)
target_include_directories(ecl_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples/common)
target_link_libraries(ecl_tests PRIVATE ecl ecl_dev_options Threads::Threads GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(ecl_tests)
