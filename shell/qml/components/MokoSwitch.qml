import QtQuick
import QtQuick.Controls

Switch {
    id: root
    implicitWidth: 43
    implicitHeight: 24
    padding: 0

    indicator: Rectangle {
        implicitWidth: 43
        implicitHeight: 24
        radius: 12
        color: !root.enabled ? "#CED7E0" : root.checked ? "#3475E7" : "#AFBBC7"
        border.color: Qt.rgba(1,1,1,.72)

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "white"
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }

    contentItem: Item {}
}
