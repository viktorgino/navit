set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(vehicle_demo MODULE vehicle_demo.cpp)

find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_definitions(${Qt5Widgets_DEFINITIONS})

target_link_libraries(vehicle_demo ${Qt5Widgets_LIBRARIES})

target_include_directories(vehicle_demo PRIVATE ${CMAKE_SOURCE_DIR}/navit ${CMAKE_SOURCE_DIR}/navit/includes)
