import QtQuick
import QtQuick.Window

Item {
    id: root
    property alias source: image.source
    property alias asynchronous: image.asynchronous
    property alias mipmap: image.mipmap
    property real implicitSize: 0
    readonly property int status: image.status
    implicitWidth: implicitSize
    implicitHeight: implicitSize
    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        sourceSize: Qt.size(Math.ceil(root.width * Screen.devicePixelRatio), Math.ceil(root.height * Screen.devicePixelRatio))
    }
}
