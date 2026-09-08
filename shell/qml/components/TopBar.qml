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
    property var notificationModel
    property var windowManager
    signal controlCenterRequested(int page)
    signal notificationCenterRequested()
    signal screenshotRequested()
    signal keyboardLayoutRequested()

    component StatusButton: Item {
        id: statusButton
        property string kind
        property string label
        property bool active: true
        property int level: 100
        property bool charging: false
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
            charging: statusButton.charging
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
            charging: root.control && root.control.batteryCharging
            onClicked: root.controlCenterRequested(3)
        }
        StatusButton {
            kind: "screenshot"
            label: "Take screenshot"
            active: true
            onClicked: root.screenshotRequested()
        }
        Item {
            width: 27
            height: 30
            StatusButton {
                anchors.fill: parent
                kind: "notification"
                label: root.notificationModel && root.notificationModel.unreadCount > 0
                       ? root.notificationModel.unreadCount + " unread notifications"
                       : "Notification Center"
                active: true
                onClicked: root.notificationCenterRequested()
            }
            Rectangle {
                visible: root.notificationModel && root.notificationModel.unreadCount > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 1
                anchors.topMargin: 2
                width: 7
                height: 7
                radius: 4
                color: "#3978F6"
                border.width: 1
                border.color: "#FFFFFF"
            }
        }
        Item {
            width: 31
            height: 30
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: layoutMouse.containsMouse || layoutMouse.pressed
                       ? Qt.rgba(.25,.42,.60,.12) : "transparent"
            }
            Text {
                anchors.centerIn: parent
                text: root.windowManager && root.windowManager.keyboardLayout === 1 ? "VI" : "EN"
                color: "#2D3948"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: layoutMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.keyboardLayoutRequested()
            }
            ToolTip.visible: layoutMouse.containsMouse
            ToolTip.text: root.windowManager ? root.windowManager.keyboardLayoutName
                                             : "Keyboard layout"
            ToolTip.delay: 450
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
                cursorShape: Qt.PointingHandCursor
                onClicked: root.notificationCenterRequested()
            }
            ToolTip.visible: clockMouse.containsMouse
            ToolTip.text: root.dateText
            ToolTip.delay: 350
        }
    }
}
