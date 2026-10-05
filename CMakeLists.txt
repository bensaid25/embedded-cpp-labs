cmake_minimum_required(VERSION 3.20)
project(embedded_cpp_labs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # use standard C++, not GNU extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # helps VS Code understand the code

option(ECL_BUILD_TESTS "Build unit tests" ON)
option(ECL_BUILD_EXAMPLES "Build host examples" ON)
option(ECL_WARNINGS_AS_ERRORS "Treat warnings as errors in tests and examples" OFF)
option(ECL_SANITIZE "Build tests and examples with ASan and UBSan" OFF)

# The library itself: headers only.
add_library(ecl INTERFACE)
target_include_directories(ecl INTERFACE include)
target_compile_features(ecl INTERFACE cxx_std_17)

# Warning and sanitizer flags for OUR code only (tests, examples), so they
# are never applied to third-party code such as GoogleTest.
add_library(ecl_dev_options INTERFACE)
target_compile_options(ecl_dev_options INTERFACE -Wall -Wextra -Wpedantic)
if(ECL_WARNINGS_AS_ERRORS)
    target_compile_options(ecl_dev_options INTERFACE -Werror)
endif()
if(ECL_SANITIZE)
    target_compile_options(ecl_dev_options INTERFACE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer)
    target_link_options(ecl_dev_options INTERFACE -fsanitize=address,undefined)
endif()

if(ECL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ECL_BUILD_EXAMPLES)
    add_subdirectory(examples/host)
endif()
