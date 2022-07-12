# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to build QRC if needed
set(CMAKE_AUTORCC ON)

# initialize QML and QWIDGET usage to not use
set(QML 0)
set(QWIDGET 0)

set(HAVE_FREETYPE 1)

find_package(Qt5 REQUIRED COMPONENTS Core Quick)

qt5_add_resources(QT5_QRC "graphics_qt5.qrc")
qt5_add_resources(GUI_QT5_QML_QRC "gui/gui_qt5_qml.qrc")

add_library (navit_graphics SHARED graphics_qt5 graphics_qt5.cpp event_qt5.cpp qt5graphicsworker.cpp navitinstance.h
    gui/gui_qt5_qml.cpp
    gui/navitpoimodel.cpp gui/navitrecentsmodel.cpp gui/navitfavouritesmodel.cpp gui/navitsearchmodel.cpp
    gui/navithelper.cpp gui/navitroute.cpp gui/navitlayoutsmodel.cpp gui/navitlayersmodel.cpp
    gui/navitvehiclesmodel.cpp gui/navitmapsmodel.cpp gui/proxy.c QNavitQuick.cpp QNavitQuick_2.cpp
    ${GUI_QT5_QML_QRC}
    ${QT5_QRC})

# pass QML and QWIDGET preference to source
target_compile_definitions(navit_graphics PRIVATE USE_QML=${QML} USE_QWIDGET=${QWIDGET} HAVE_FREETYPE=${HAVE_FREETYPE})
