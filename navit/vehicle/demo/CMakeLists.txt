add_library(vehicle_demo SHARED vehicle_demo.cpp)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GMODULE REQUIRED IMPORTED_TARGET gmodule-2.0)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.10)

target_link_libraries(vehicle_demo PkgConfig::GMODULE PkgConfig::GLIB2 Qt5::Core Qt5::Quick)

target_include_directories(vehicle_demo PRIVATE ${CMAKE_SOURCE_DIR}/navit ${CMAKE_SOURCE_DIR}/navit/includes)