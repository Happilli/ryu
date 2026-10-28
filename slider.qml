import QtQuick
import QtQuick.Window
import Ryu.Extraa

Window {
    id: win
    width: 1000
    height: 820
    visible: true
    title: "MaterialSlider"
    color: dark ? "#141218" : "#FEF7FF"

    property bool dark: true
    readonly property color fg: dark ? "#E6E0E9" : "#1D1B20"
    readonly property color dim: dark ? "#938F99" : "#49454F"
    readonly property color accent: dark ? "#D0BCFF" : "#6750A4"
    readonly property color track: dark ? "#4A4458" : "#E8DEF8"
    readonly property color onAccent: dark ? "#381E72" : "#FFFFFF"

    property int frames: 0
    property int fps: 0
    onFrameSwapped: frames++
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            win.fps = win.frames;
            win.frames = 0;
        }
    }

    component Label: Text {
        color: win.dim
        font.pixelSize: 12
    }

    component S: MaterialSlider {
        color: win.accent
        handleColor: win.accent
        trackColor: win.track
        iconColor: win.onAccent
    }
    S {
        id: rs
        sizeStyle: MaterialSlider.Medium
        value: 0.5
        SequentialAnimation on width {
            loops: Animation.Infinite
            NumberAnimation {
                from: 100
                to: 900
                duration: 2500
            }
            NumberAnimation {
                from: 900
                to: 100
                duration: 2500
            }
        }
    }
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.height + 48
        clip: true

        Column {
            id: col
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 20

            Row {
                spacing: 16
                Text {
                    text: "fps: " + win.fps
                    color: win.fg
                    font.pixelSize: 16
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    text: "dark"
                    anchors.verticalCenter: parent.verticalCenter
                }
                MaterialSwitch {
                    checked: win.dark
                    onToggled: win.dark = checked
                }
                Label {
                    text: "master value: " + master.value.toFixed(2)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Label {
                text: "Size styles XS / S / M / L / XL (drag: bubble pops, text should stay sharp)"
            }
            Column {
                spacing: 4
                Repeater {
                    model: 5
                    S {
                        required property int modelData
                        width: 520
                        sizeStyle: modelData
                        value: 0.5
                    }
                }
            }

            Label {
                text: "Master slider (drives the others), stepSize 0.1 with stops"
            }
            S {
                id: master
                width: 520
                sizeStyle: MaterialSlider.Medium
                stepSize: 0.1
                value: 0.4
            }

            Label {
                text: "Mirrors of master (value set via binding), no stops / no indicator"
            }
            Row {
                spacing: 24
                S {
                    width: 240
                    sizeStyle: MaterialSlider.Small
                    showStops: false
                    value: master.value
                    enabled: false
                }
                S {
                    width: 240
                    sizeStyle: MaterialSlider.Small
                    showStops: false
                    showValueIndicator: false
                    value: master.value
                    onMoved: master.value = value
                }
            }

            Label {
                text: "Range 0..100, step 5, decimals 0; range -1..1 with 2 decimals"
            }
            Row {
                spacing: 24
                S {
                    width: 300
                    sizeStyle: MaterialSlider.Medium
                    from: 0
                    to: 100
                    stepSize: 5
                    value: 35
                }
                S {
                    width: 300
                    sizeStyle: MaterialSlider.Medium
                    from: -1
                    to: 1
                    decimals: 2
                    showStops: false
                    value: 0.25
                }
            }

            Label {
                text: "Icons (glyph text), large sizes: icon must stay inside the active part"
            }
            Column {
                spacing: 4
                S {
                    width: 520
                    sizeStyle: MaterialSlider.Large
                    iconText: "♪"
                    iconSize: 24
                    value: 0.6
                }
                S {
                    width: 520
                    sizeStyle: MaterialSlider.ExtraLarge
                    iconText: "★"
                    iconSize: 32
                    value: 0.7
                }
                S {
                    width: 520
                    sizeStyle: MaterialSlider.Large
                    iconText: "♪"
                    iconSize: 24
                    value: 0.05
                }
            }

            Label {
                text: "Border + custom geometry + custom outer radius"
            }
            Row {
                spacing: 24
                S {
                    width: 300
                    sizeStyle: MaterialSlider.Medium
                    borderWidth: 2
                    borderColor: win.accent
                    color: "transparent"
                    value: 0.5
                }
                S {
                    width: 300
                    sizeStyle: MaterialSlider.Medium
                    trackHeight: 30
                    handleWidth: 8
                    handleHeight: 56
                    handleGap: 10
                    innerRadius: 8
                    outerRadius: 6
                    value: 0.5
                }
            }

            Label {
                text: "Vertical (bubble should appear to the left, text upright)"
            }
            Row {
                spacing: 24
                Repeater {
                    model: [0, 1, 2, 3]
                    S {
                        required property int modelData
                        orientation: MaterialSlider.Vertical
                        width: 120
                        height: 240
                        sizeStyle: modelData
                        value: 0.3 + modelData * 0.2
                    }
                }
                S {
                    orientation: MaterialSlider.Vertical
                    width: 120
                    height: 240
                    sizeStyle: MaterialSlider.Large
                    iconText: "♪"
                    iconSize: 24
                    value: 0.7
                }
            }

            Label {
                text: "Stress: 25 sliders all following master (watch fps while dragging master)"
            }
            Column {
                spacing: 2
                Repeater {
                    model: 25
                    S {
                        width: 600
                        sizeStyle: MaterialSlider.ExtraSmall
                        showValueIndicator: false
                        showStops: false
                        value: master.value
                    }
                }
            }
        }
    }
}
