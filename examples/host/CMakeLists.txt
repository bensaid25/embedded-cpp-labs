add_executable(sensor_pipeline 01_sensor_pipeline.cpp)
target_link_libraries(sensor_pipeline PRIVATE ecl ecl_dev_options)

add_executable(vibration_demo 02_vibration_demo.cpp)
target_include_directories(vibration_demo PRIVATE ../common)
target_link_libraries(vibration_demo PRIVATE ecl ecl_dev_options)
