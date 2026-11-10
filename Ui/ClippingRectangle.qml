import QtQuick

Item {
    id: root

    component Border: QtObject {
        property color color: "transparent"
        property real width: 0
    }

    property color color: "transparent"
    property real radius: 0
    property real topLeftRadius: -1
    property real topRightRadius: -1
    property real bottomLeftRadius: -1
    property real bottomRightRadius: -1
    property bool contentInsideBorder: true
    readonly property Border border: Border {}

    default property alias contents: contentItem.data
    readonly property alias contentItem: contentItem

    Item {
        id: contentHolder
        anchors.fill: parent

        Item {
            id: contentItem
            anchors.fill: parent
            anchors.margins: root.contentInsideBorder ? root.border.width : 0
        }
    }

    ShaderEffectSource {
        id: source
        hideSource: true
        sourceItem: contentHolder
        live: true
    }

    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/Ryu/Ui/shaders/rounded.frag.qsb"
        property var content: source
        property vector2d dims: Qt.vector2d(root.width, root.height)
        property vector4d radii: Qt.vector4d(root.topLeftRadius >= 0 ? root.topLeftRadius : root.radius, root.topRightRadius >= 0 ? root.topRightRadius : root.radius, root.bottomRightRadius >= 0 ? root.bottomRightRadius : root.radius, root.bottomLeftRadius >= 0 ? root.bottomLeftRadius : root.radius)
        property color backgroundColor: root.color
        property color borderColor: root.border.color
        property real borderWidth: root.border.width
    }
}
