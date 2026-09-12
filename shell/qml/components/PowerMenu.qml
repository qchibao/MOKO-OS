import QtQuick
import QtQuick.Controls

FocusScope {
    id: root
    property var control
    readonly property bool busy: control
                                 && (control.powerActionPending || control.suspendPending)
    property bool presentationReported: false
    signal dismissRequested()
    signal sleepRequested()
    signal restartRequested()
    signal shutdownRequested()
    signal presentationReady()

    focus: visible
    opacity: visible ? 1 : 0

    function focusDefaultAction() {
        Qt.callLater(function() {
            if (root.visible && shutdownButton.enabled)
                shutdownButton.forceActiveFocus()
        })
    }

    function reportPresentationReady() {
        if (!root.visible || root.opacity < 0.999 || root.presentationReported)
            return
        root.presentationReported = true
        root.focusDefaultAction()
        root.presentationReady()
    }

    function activateFocusedAction() {
        if (root.busy)
            return
        if (sleepButton.activeFocus && sleepButton.enabled) {
            root.sleepRequested()
        } else if (restartButton.activeFocus && restartButton.enabled) {
            root.restartRequested()
        } else if (cancelButton.activeFocus && cancelButton.enabled) {
            root.dismissRequested()
        } else if (shutdownButton.enabled) {
            root.shutdownRequested()
        }
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
                onClicked: root.dismissRequested()
            }
        }
    }

    Shortcut {
        sequence: "Return"
        enabled: root.visible && !root.busy
        context: Qt.WindowShortcut
        onActivated: root.activateFocusedAction()
    }

    Shortcut {
        sequence: "Enter"
        enabled: root.visible && !root.busy
        context: Qt.WindowShortcut
        onActivated: root.activateFocusedAction()
    }

    Keys.onEscapePressed: (event) => {
        event.accepted = true
        if (!root.busy)
            root.dismissRequested()
    }

    onVisibleChanged: {
        presentationReported = false
        if (visible) {
            focusDefaultAction()
            Qt.callLater(reportPresentationReady)
        }
    }

    onOpacityChanged: Qt.callLater(reportPresentationReady)

    onBusyChanged: {
        if (visible && !busy)
            focusDefaultAction()
    }

    Behavior on opacity {
        NumberAnimation { duration: 140 }
    }
}
