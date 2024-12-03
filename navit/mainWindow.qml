import QtQuick 2.0
import QtQuick.Window 2.11
import NavitGUI 1.0
import Qt.labs.settings 1.0

Window {
    id: window
    width: 1200
    height: 800
    visible: true
    title: qsTr("Navit")

    MainLayout {
        id: root
        anchors.fill: parent
    }
    Settings {
        id:windowSettings
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
    }
}
