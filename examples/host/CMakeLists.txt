add_executable(sensor_pipeline 01_sensor_pipeline.cpp)
target_link_libraries(sensor_pipeline PRIVATE ecl)
target_compile_options(sensor_pipeline PRIVATE -Wall -Wextra -Wpedantic)
