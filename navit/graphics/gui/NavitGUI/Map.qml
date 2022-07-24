

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick3D 1.15

//import Map 1.0

Rectangle {

    color: "#c2c2c2"

    View3D {
        id: view3D
        anchors.fill: parent

        environment: sceneEnvironment

        SceneEnvironment {
            id: sceneEnvironment
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        Node {
            id: scene
            DirectionalLight {
                id: directionalLight
                x: 0
                y: 432.934
                eulerRotation.x: -90
                z: 0
            }

            PerspectiveCamera {
                id: sceneCamera
                x: 0.6
                y: 40.688
                fieldOfView: 5
                clipFar: 37210
                eulerRotation.z: 0
                eulerRotation.y: 0
                eulerRotation.x: -20
                z: 93.89654
            }
            Model {
                x: 0
                y: 0
                source: "#Rectangle"
                z: 0
                eulerRotation.x: -90
                 materials: newMaterial
            }
//            PerspectiveCamera {
//                id: sceneCamera
//                x: 2453.598
//                y: 1436.808
//                fieldOfView: 5
//                clipFar: 37210
//                eulerRotation.z: 0
//                eulerRotation.y: 0
//                eulerRotation.x: -90
//                z: 3662.53491
//            }
//            Repeater3D {
//                id: tileRepeater
//                property int rows: 64
//                property int columns: 64
//                model: rows * columns
//                Model {
//                    x: Math.floor(index % tileRepeater.columns * 100)
//                    y: 0
//                    source: "#Rectangle"
//                    z: Math.floor(index / tileRepeater.columns) * 100
//                    eulerRotation.x: -90
//                    // materials: newMaterial
//                    materials:  DefaultMaterial {
//                        diffuseMap: Texture{
//                            textureData: MapTexture {
//                                width: 256
//                                height: width
//                            }
//                        }
//                    }
//                }
//            }
        }
    }

    Item {
        id: __materialLibrary__
        DefaultMaterial {
            id: defaultMaterial
            objectName: "Default Material"
            diffuseColor: "#4aee45"
        }

        PrincipledMaterial {
            id: mapMaterial
            objectName: "Map Material"
        }

       PrincipledMaterial {
           id: newMaterial
           objectName: "New Material"
           baseColorMap: Texture {
               id: texture2s
               source: "navit.png"
               scaleV: 1
               scaleU: 1
               mappingMode: Texture.UV
               tilingModeVertical: Texture.ClampToEdge
               tilingModeHorizontal: Texture.ClampToEdge
           }
       }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        property int xOrigin : 0
        property int yOrigin : 0
        property int xTiltOrigin : 0
        property int yTiltOrigin : 0
        property int xCameraOrigin : 0
        property int zCameraOrigin : 0
        onPositionChanged : (mouse)=>{
                                var deltaY = yOrigin - mouse.y
                                var deltaX = xOrigin - mouse.x
                                if(mouse.modifiers & Qt.ShiftModifier){
                                    sceneCamera.eulerRotation.x = xTiltOrigin - Math.floor(deltaY / 2)
                                    sceneCamera.eulerRotation.y = yTiltOrigin - Math.floor(deltaX / 2)
                                } else {
                                    sceneCamera.x = xCameraOrigin + deltaX
                                    sceneCamera.z = zCameraOrigin + deltaY
                                }
                            }
        onPressed: (mouse)=> {
                       yOrigin = mouse.y
                       xOrigin = mouse.x
                       xTiltOrigin = sceneCamera.eulerRotation.x
                       yTiltOrigin = sceneCamera.eulerRotation.y
                       xCameraOrigin = sceneCamera.x
                       zCameraOrigin = sceneCamera.z
                   }
    }

    Text {
        text: qsTr("Hello Map3d")
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 100
    }

}

/*##^##
Designer {
    D{i:0;height:500;width:800}D{i:4}D{i:5}D{i:6}D{i:3}D{i:12}
}
##^##*/

