import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string title: "App"
    property string kind: "system"
    property bool compact: false
    signal activated()

    width: compact ? 62 : 76
    height: compact ? 62 : 96

    Column {
        anchors.centerIn: parent
        spacing: compact ? 0 : 5

        Rectangle {
            id: tile
            width: root.compact ? 56 : 62
            height: width
            radius: 16
            color: mouse.containsMouse ? Qt.rgba(1,1,1,.94) : Qt.rgba(.97,.99,1,.80)
            border.width: 1
            border.color: Qt.rgba(1,1,1,.86)
            scale: mouse.pressed ? .94 : (mouse.containsMouse ? 1.05 : 1.0)

            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

            MokoGlyph {
                anchors.fill: parent
                anchors.margins: 9
                kind: root.kind
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.activated()
            }
        }

        Text {
            visible: !root.compact
            width: 76
            text: root.title
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#263342"
            font.pixelSize: 11
        }
    }
}
