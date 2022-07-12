cmake_minimum_required(VERSION 3.14)

project(navit VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory (fib-1.1)
add_subdirectory (graphics)

find_package(PkgConfig REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Quick)


pkg_check_modules(GMODULE REQUIRED IMPORTED_TARGET gmodule-2.0)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.10)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig>=2.2.0)
pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1>=1.4)
pkg_check_modules(DBUSGLIB REQUIRED IMPORTED_TARGET dbus-glib-1)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)

set(QT_LIBS Qt5::Core Qt5::Quick PkgConfig::GMODULE PkgConfig::GLIB2 ${GLIB_LIBRARIES} PkgConfig::FONTCONFIG PkgConfig::DBUS PkgConfig::DBUSGLIB PkgConfig::ZLIB fib)

add_library (navit_core SHARED announcement.c atom.c attr.c cache.c callback.c command.c config_.c coord.c country.c data_window.c debug.c
event.c file.c geom.c graphics.c gui.c item.c layout.c log.c main.c map.c maps.c
linguistics.c mapset.c maptype.c menu.c messages.c bookmarks.c navit.c navit_nls.c navigation.c osd.c param.c phrase.c plugin.c popup.c
profile.c profile_option.c projection.c roadprofile.c route.c script.c search.c speech.c start_real.c sunriset.c transform.c track.c
search_houseno_interpol.c traffic.c util.c vehicle.c vehicleprofile.c xmlconfig.c)

target_compile_definitions(navit_core PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(navit_core ${QT_LIBS})
target_link_libraries(navit_graphics ${QT_LIBS})
target_include_directories(navit_core PRIVATE ${GLIB_INCLUDE_DIRS})
link_directories (${GTK2_LIBRARY_DIRS})
add_definitions (${GTK2_CFLAGS_OTHER})

# target_link_libraries(navit_core INTERFACE ${GLIB_LDFLAGS})

set_target_properties(navit_core PROPERTIES COMPILE_DEFINITIONS "MODULE=navit;LIBDIR=\"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}\";PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
set_target_properties(navit_core PROPERTIES COMPILE_DEFINITIONS "MODULE=navit;LIBDIR=\"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}\";PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")

set(XMLCFG_ALL_FILE_PREFIXES "navit;navit_layout_car;navit_layout_car_dark;navit_layout_car_android;navit_layout_car_simple;navit_layout_bike;navit_layout_th")
set(XML_FILES "")
foreach(XMLFILE_PREFIX ${XMLCFG_ALL_FILE_PREFIXES})
    set(XML_FILES ${XML_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/configs/${XMLFILE_PREFIX}.xml)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/${XMLFILE_PREFIX}.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${XMLFILE_PREFIX}.xml)
endforeach()
add_custom_target(navit_config_xml ALL COMMAND cp ${XML_FILES} ${CMAKE_BINARY_DIR})

qt5_add_resources(NAVIT_QRC "navit.qrc")

add_executable(navit start.cpp ${NAVIT_QRC})

target_link_libraries (navit navit_core Qt5::Core Qt5::Gui Qt5::Quick)
