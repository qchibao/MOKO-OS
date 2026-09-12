import QtQuick
import QtQuick.Controls

FocusScope {
    id: root
    property var control
    readonly property bool busy: control
                                 && (control.powerActionPending || control.suspendPending)
    signal dismissRequested()
    signal sleepRequested()
    signal restartRequested()
    signal shutdownRequested()

    focus: visible
    opacity: visible ? 1 : 0

    function focusDefaultAction() {
        Qt.callLater(function() {
            if (root.visible && shutdownButton.enabled)
                shutdownButton.forceActiveFocus()
        })
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.02, 0.05, 0.10, 0.58)
        MouseArea {
            anchors.fill: parent
            onClicked: {
                mouse.accepted = true
                if (!root.busy)
                    root.dismissRequested()
            }
        }
    }

    GlassPanel {
        id: panel
        anchors.centerIn: parent
        width: Math.min(460, root.width - 40)
        height: 252
        glassOpacity: 0.96

        MouseArea {
            anchors.fill: parent
            onClicked: mouse.accepted = true
        }

        Column {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 16

            Text {
                text: "Power"
                color: panel.appearanceMode === "dark" ? "#F6FAFF" : "#152238"
                font.pixelSize: 24
                font.bold: true
            }

            Text {
                text: root.busy && root.control.operationMessage.length > 0
                      ? root.control.operationMessage
                      : "Choose what you want to do with this computer."
                color: panel.appearanceMode === "dark" ? "#B8C8DB" : "#5C6C80"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Row {
                width: parent.width
                spacing: 8

                MokoActionButton {
                    id: sleepButton
                    width: (parent.width - 16) / 3
                    text: "Sleep"
                    enabled: root.control && root.control.suspendAvailable && !root.busy
                    KeyNavigation.right: restartButton
                    KeyNavigation.down: cancelButton
                    KeyNavigation.tab: restartButton
                    KeyNavigation.backtab: cancelButton
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            event.accepted = true
                            root.sleepRequested()
                        }
                    }
                    onClicked: root.sleepRequested()
                }

                MokoActionButton {
                    id: restartButton
                    width: (parent.width - 16) / 3
                    text: "Restart"
                    enabled: root.control && !root.busy
                    KeyNavigation.left: sleepButton
                    KeyNavigation.right: shutdownButton
                    KeyNavigation.down: cancelButton
                    KeyNavigation.tab: shutdownButton
                    KeyNavigation.backtab: sleepButton
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            event.accepted = true
                            root.restartRequested()
                        }
                    }
                    onClicked: root.restartRequested()
                }

                MokoActionButton {
                    id: shutdownButton
                    width: (parent.width - 16) / 3
                    text: "Shut Down"
                    accent: true
                    enabled: root.control && !root.busy
                    KeyNavigation.left: restartButton
                    KeyNavigation.down: cancelButton
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: restartButton
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            event.accepted = true
                            root.shutdownRequested()
                        }
                    }
                    onClicked: root.shutdownRequested()
                }
            }

            MokoActionButton {
                id: cancelButton
                anchors.right: parent.right
                text: "Cancel"
                enabled: !root.busy
                KeyNavigation.up: shutdownButton
                KeyNavigation.tab: sleepButton
                KeyNavigation.backtab: shutdownButton
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        event.accepted = true
                        root.dismissRequested()
                    }
                }
                onClicked: root.dismissRequested()
            }
        }
    }

    Keys.onEscapePressed: (event) => {
        event.accepted = true
        if (!root.busy)
            root.dismissRequested()
    }

    onVisibleChanged: {
        if (visible)
            focusDefaultAction()
    }

    onBusyChanged: {
        if (visible && !busy)
            focusDefaultAction()
    }

    Behavior on opacity {
        NumberAnimation { duration: 140 }
    }
}
