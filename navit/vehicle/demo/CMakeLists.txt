add_library(vehicle_demo SHARED vehicle_demo.cpp)

target_include_directories(vehicle_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../)