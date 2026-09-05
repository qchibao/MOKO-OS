import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string title: "App"
    property string kind: "system"
    property url iconSource
    property string launchState: "ready"
    property string launchMessage
    property bool compositorRunning: false
    property bool compact: false
    property bool dense: false
    property bool selected: false
    property int compactExtent: 62
    signal activated()

    width: compact ? compactExtent : (dense ? 68 : 76)
    height: compact ? compactExtent : (dense ? 75 : 96)

    Column {
        anchors.centerIn: parent
        spacing: compact ? 0 : 5

        Rectangle {
            id: tile
            width: root.compact ? root.compactExtent - 6 : (root.dense ? 52 : 62)
            height: width
            radius: root.dense ? 14 : 16
            color: mouse.containsMouse ? Qt.rgba(1,1,1,.94) : Qt.rgba(.97,.99,1,.80)
            border.width: root.selected ? 2 : 1
            border.color: root.selected ? "#3F7CFF" : Qt.rgba(1,1,1,.86)
            scale: mouse.pressed ? .94 : (mouse.containsMouse ? 1.05 : 1.0)

            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

            MokoGlyph {
                anchors.fill: parent
                anchors.margins: 9
                kind: root.kind
                visible: appIcon.status !== Image.Ready
            }

            Image {
                id: appIcon
                anchors.fill: parent
                anchors.margins: 8
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
            }

            Rectangle {
                visible: root.compositorRunning || root.launchState !== "ready"
                width: root.compact ? 9 : 11
                height: width
                radius: width / 2
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: root.compositorRunning || root.launchState === "running" ? "#2671D9"
                      : root.launchState === "failed" ? "#D94A57" : "#E8A326"
                border.width: 2
                border.color: "white"
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.activated()
            }

            ToolTip.visible: root.compact && mouse.containsMouse
            ToolTip.text: root.title
        }

        Text {
            visible: !root.compact
            width: root.dense ? 68 : 76
            text: root.title
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#263342"
            font.pixelSize: root.dense ? 10 : 11
        }
    }

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: launchMessage
}
