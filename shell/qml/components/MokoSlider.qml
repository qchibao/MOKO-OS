import QtQuick
import QtQuick.Controls

Slider {
    id: root
    implicitHeight: 28

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 5
        radius: 2.5
        color: "#CFD9E4"

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: "#3475E7"
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 18
        height: 18
        radius: 9
        color: root.pressed ? "#EAF3FF" : "white"
        border.width: 1
        border.color: "#9DB7D2"
    }
}
