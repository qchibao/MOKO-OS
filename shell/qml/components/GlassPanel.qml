import QtQuick

Rectangle {
    id: root
    property real glassOpacity: 0.72
    property color glassColor: "#F8FCFF"

    radius: 24
    color: Qt.rgba(glassColor.r, glassColor.g, glassColor.b, glassOpacity)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.82)

    layer.enabled: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, parent.radius - 1)
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(0.45, 0.65, 0.85, 0.12)
    }
}
