add_library(vehicle_demo SHARED vehicle_demo.cpp)


target_link_libraries(vehicle_demo Qt5::Core)

target_include_directories(vehicle_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../../)