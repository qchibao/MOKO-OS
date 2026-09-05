import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool accent: false
    property bool danger: false

    implicitHeight: 34
    implicitWidth: Math.max(72, contentItem.implicitWidth + 24)
    leftPadding: 12
    rightPadding: 12

    contentItem: Text {
        text: root.text
        color: !root.enabled ? "#98A6B5"
              : root.accent ? "white"
              : root.danger ? "#A63B48" : "#334252"
        font.pixelSize: 11
        font.bold: root.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 7
        color: !root.enabled ? Qt.rgba(.75,.80,.85,.24)
              : root.accent ? (root.down ? "#285FC0" : "#3475E7")
              : root.danger ? (root.down ? Qt.rgba(.75,.18,.24,.17)
                                         : Qt.rgba(.86,.25,.30,.09))
              : root.down ? Qt.rgba(.22,.35,.48,.14)
                          : root.hovered ? Qt.rgba(1,1,1,.82) : Qt.rgba(1,1,1,.55)
        border.width: root.accent ? 0 : 1
        border.color: root.danger ? Qt.rgba(.70,.20,.25,.22) : Qt.rgba(.35,.50,.65,.15)
    }
}
