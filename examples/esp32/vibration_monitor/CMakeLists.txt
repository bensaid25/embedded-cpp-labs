# ESP-IDF project: vibration monitor using the ecl library and an MPU6050.
cmake_minimum_required(VERSION 3.16)

# Makes the "ecl" component (a thin wrapper around ../../../include) visible.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vibration_monitor)
