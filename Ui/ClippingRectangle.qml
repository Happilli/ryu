pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects

Item {
    id: root
    property real radius: 0
    property alias color: bg.color
    property alias border: frame.border
    default property alias contentData: content.data
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.radius
    }
    Item {
        id: content
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: maskItem
        }
    }
    Item {
        id: maskItem
        anchors.fill: parent
        visible: false
        layer.enabled: true
        layer.smooth: true
        Rectangle {
            anchors.fill: parent
            anchors.margins: frame.border.width
            radius: Math.max(0, root.radius - frame.border.width)
        }
    }
    Rectangle {
        id: frame
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
    }
}
