cmake_minimum_required(VERSION 3.20)
project(embedded_cpp_labs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECL_BUILD_TESTS "Build unit tests" ON)

add_library(ecl INTERFACE)
target_include_directories(ecl INTERFACE include)
target_compile_features(ecl INTERFACE cxx_std_17)

if(ECL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()