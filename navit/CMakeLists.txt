cmake_minimum_required(VERSION 3.14)

project(navit VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory (fib-1.1)
add_subdirectory (graphics)
add_subdirectory (map/binfile)
add_subdirectory (map/textfile)
add_subdirectory (vehicle/demo)

find_package(PkgConfig REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Quick Svg Quick3D)

pkg_check_modules(GMODULE REQUIRED IMPORTED_TARGET gmodule-2.0)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.10)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig>=2.2.0)
pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1>=1.4)
pkg_check_modules(DBUSGLIB REQUIRED IMPORTED_TARGET dbus-glib-1)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)

set(NAVIT_LIBS Qt5::Core Qt5::Quick Qt5::Svg Qt5::Quick3D
    PkgConfig::GMODULE PkgConfig::GLIB2
    ${GLIB_LIBRARIES}
    PkgConfig::FONTCONFIG PkgConfig::DBUS 
    PkgConfig::DBUSGLIB PkgConfig::ZLIB fib)

add_library (navit_core SHARED announcement.c atom.c attr.c cache.c callback.c command.c config_.c coord.c country.c data_window.c debug.c
event.c file.c geom.c graphics.cpp graphics_displaylist.cpp item.c log.c main.c map.c maps.c
linguistics.c mapset.c maptype.c messages.c bookmarks.c navit.cpp navit_wrapper.cpp navit_nls.c navigation.c param.c phrase.c plugin.c
profile.c profile_option.c projection.c roadprofile.c route.c search.c speech.c sunriset.c transform.c track.c transform_2.cpp
search_houseno_interpol.c traffic.c util.c vehicle.cpp vehicle_wrapper.cpp vehicleprofile.c xmlconfig.c start_real.cpp includes/NavitConfig.h config_loader.cpp plugin_loader.cpp config_loader_navit.h config_loader_layout.h)

target_compile_definitions(navit_core PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(navit_core ${NAVIT_LIBS})
link_directories (${GTK2_LIBRARY_DIRS})
add_definitions (${GTK2_CFLAGS_OTHER})

target_include_directories(navit_core PRIVATE ${GLIB_INCLUDE_DIRS})
target_include_directories(navit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/graphics/ ${CMAKE_CURRENT_SOURCE_DIR}/includes/)

# target_link_libraries(navit_core INTERFACE ${GLIB_LDFLAGS})

set_target_properties(navit_core PROPERTIES COMPILE_DEFINITIONS "MODULE=navit;LIBDIR=\"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}\";PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")

# Install all XML files
set(XMLCFG_ALL_FILE_PREFIXES "navit;navit_layout_car;navit_layout_car_dark;navit_layout_car_android;navit_layout_car_simple;navit_layout_bike;navit_layout_th;navit_layout_car_modern")
set(XML_FILES "")
foreach(XMLFILE_PREFIX ${XMLCFG_ALL_FILE_PREFIXES})
    set(XML_FILES ${XML_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/config/${XMLFILE_PREFIX}.xml)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/${XMLFILE_PREFIX}.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${XMLFILE_PREFIX}.xml)
endforeach()
message("Copying files: ${XML_FILES}")
add_custom_target(navit_config_xml ALL DEPENDS COMMAND cp ${XML_FILES} ${CMAKE_BINARY_DIR})

# Install all JSON files
set(JSONCFG_ALL_FILE_PREFIXES "navit;navit_layout_car_modern")
set(JSON_FILES "")
foreach(JSONFILE_PREFIX ${JSONCFG_ALL_FILE_PREFIXES})
    set(JSON_FILES ${JSON_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/config/${JSONFILE_PREFIX}.json)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/${JSONFILE_PREFIX}.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${JSONFILE_PREFIX}.json)
endforeach()
message("Copying files: ${JSON_FILES}")
add_custom_target(navit_config_json ALL DEPENDS COMMAND cp ${JSON_FILES} ${CMAKE_BINARY_DIR})


target_link_libraries(graphics_qt5 navit_core)
target_link_libraries(map_binfile navit_core)
target_link_libraries(map_textfile navit_core)
target_link_libraries(vehicle_demo navit_core)

Qt5_add_resources(NAVIT_QRC "navit.qrc")

add_executable(navit_app start_real.cpp ${NAVIT_QRC})
target_link_libraries(navit_app PRIVATE navit_core ${NAVIT_LIBS})
# target_link_libraries(navit_app INTERFACE graphics_qt5 vehicle_demo map_binfile)
target_include_directories(navit_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/graphics/ ${CMAKE_CURRENT_SOURCE_DIR}/includes/)

add_custom_command(TARGET navit_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:navit_app> ${CMAKE_BINARY_DIR}
)