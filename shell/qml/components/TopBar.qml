import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    height: 40
    color: Qt.rgba(.94,.98,1,.74)
    border.color: Qt.rgba(1,1,1,.64)

    property string clockText: "--:--"
    property string dateText: ""
    property var control
    signal controlCenterRequested(int page)

    component StatusButton: Item {
        id: statusButton
        property string kind
        property string label
        property bool active: true
        property int level: 100
        signal clicked()
        width: 27
        height: 30

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: statusMouse.containsMouse || statusMouse.pressed
                   ? Qt.rgba(.25,.42,.60,.12) : "transparent"
        }
        StatusIcon {
            anchors.centerIn: parent
            width: 17
            height: 17
            kind: statusButton.kind
            active: statusButton.active
            level: statusButton.level
        }
        MouseArea {
            id: statusMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: statusButton.clicked()
        }
        ToolTip.visible: statusMouse.containsMouse
        ToolTip.text: statusButton.label
        ToolTip.delay: 450
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18

        Text {
            text: "MOKO"
            color: "#11151A"
            font.pixelSize: 18
            font.bold: true
            font.letterSpacing: 0
        }
        Rectangle {
            width: 1
            height: 17
            color: "#C5D5E1"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "Hardware & Usability Preview"
            color: "#536477"
            font.pixelSize: 11
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        StatusButton {
            kind: "brightness"
            label: root.control && root.control.brightnessAvailable
                   ? "Brightness " + root.control.brightness + "%" : "Display"
            active: root.control && root.control.brightnessAvailable
            onClicked: root.controlCenterRequested(2)
        }
        StatusButton {
            kind: "bluetooth"
            label: root.control && root.control.bluetoothPowered ? "Bluetooth on" : "Bluetooth off"
            active: root.control && root.control.bluetoothPowered
            onClicked: root.controlCenterRequested(0)
        }
        StatusButton {
            kind: "wifi"
            label: root.control && root.control.activeSsid
                   ? "Wi-Fi: " + root.control.activeSsid
                   : root.control && root.control.wifiEnabled ? "Wi-Fi on" : "Wi-Fi off"
            active: root.control && root.control.wifiEnabled
            onClicked: root.controlCenterRequested(0)
        }
        StatusButton {
            kind: "sound"
            label: root.control && root.control.outputMuted
                   ? "Sound muted" : root.control && root.control.audioAvailable
                     ? "Volume " + root.control.outputVolume + "%" : "Sound unavailable"
            active: root.control && root.control.audioAvailable && !root.control.outputMuted
            level: root.control ? root.control.outputVolume : 0
            onClicked: root.controlCenterRequested(1)
        }
        StatusButton {
            kind: "battery"
            label: root.control && root.control.batteryAvailable
                   ? "Battery " + root.control.batteryPercent + "%" : "Power"
            active: root.control && root.control.batteryAvailable
            level: root.control && root.control.batteryAvailable ? root.control.batteryPercent : 100
            onClicked: root.controlCenterRequested(3)
        }
        Item { width: 6; height: 1 }
        Item {
            width: clockLabel.implicitWidth + 12
            height: 30
            Text {
                id: clockLabel
                anchors.centerIn: parent
                text: root.clockText
                color: "#263342"
                font.pixelSize: 12
            }
            MouseArea {
                id: clockMouse
                anchors.fill: parent
                hoverEnabled: true
            }
            ToolTip.visible: clockMouse.containsMouse
            ToolTip.text: root.dateText
            ToolTip.delay: 350
        }
    }
}
