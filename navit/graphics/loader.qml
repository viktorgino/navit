import QtQuick 2.2
import QtQuick.Window 2.0

Window {
    width: 200; height: 200
    Loader {
        anchors.fill: parent
        id: navit_loader
        source: "qrc:/Navit/MainLayout.qml"
    }
}
