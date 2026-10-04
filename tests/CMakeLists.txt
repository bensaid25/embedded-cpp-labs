include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(ecl_tests test_sanity.cpp test_sensor_reading.cpp test_ring_buffer.cpp test_statistics.cpp)
target_link_libraries(ecl_tests PRIVATE ecl GTest::gtest_main)
target_compile_options(ecl_tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(ecl_tests)