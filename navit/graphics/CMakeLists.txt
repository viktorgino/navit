# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to build QRC if needed
set(CMAKE_AUTORCC ON)

# initialize QML and QWIDGET usage to not use
set(QML 0)
set(QWIDGET 0)

set(HAVE_FREETYPE 0)

find_package(Qt5 REQUIRED COMPONENTS Core Quick)

qt5_add_resources(QT5_QRC "graphics_qt5.qrc")
qt5_add_resources(GUI_QT5_QML_QRC "gui/gui_qt5_qml.qrc")

add_library (graphics_qt5 SHARED graphics_qt5 graphics_qt5.cpp event_qt5.cpp qt5graphicsworker.cpp navitinstance.h
    gui/gui_qt5_qml.cpp
    gui/navitpoimodel.cpp gui/navitrecentsmodel.cpp gui/navitfavouritesmodel.cpp gui/navitsearchmodel.cpp
    gui/navithelper.cpp gui/navitroute.cpp gui/navitlayoutsmodel.cpp gui/navitlayersmodel.cpp
    gui/navitvehiclesmodel.cpp gui/navitmapsmodel.cpp gui/proxy.c QNavitQuick.cpp QNavitQuick_2.cpp
    ${GUI_QT5_QML_QRC}
    ${QT5_QRC})


target_include_directories(graphics_qt5 PRIVATE "../" "../../")
set_target_properties(graphics_qt5 PROPERTIES COMPILE_DEFINITIONS "MODULE=graphics_qt5")
# pass QML and QWIDGET preference to source

add_custom_target(graphics_qml_files ALL DEPENDS COMMAND cp -R ${CMAKE_CURRENT_SOURCE_DIR}/gui/NavitGUI ${CMAKE_BINARY_DIR})
add_custom_target(qraphics_qmldir ALL DEPENDS COMMAND cp -R ${CMAKE_CURRENT_SOURCE_DIR}/qmldir ${CMAKE_BINARY_DIR}/graphics)

target_compile_definitions(graphics_qt5 PRIVATE USE_QML=${QML} USE_QWIDGET=${QWIDGET} HAVE_FREETYPE=${HAVE_FREETYPE})
