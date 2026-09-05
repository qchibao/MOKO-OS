import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Moko.Terminal 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 1040
    height: 680
    minimumWidth: 640
    minimumHeight: 420
    title: terminal.terminalTitle
    color: "#181C22"
    flags: Qt.Window | Qt.FramelessWindowHint

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#EEF6FC"
            border.color: "#C9DAE7"

            MouseArea {
                anchors.fill: parent
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 12
                spacing: 10

                Text { text: "MOKO"; color: "#10151C"; font.pixelSize: 18; font.weight: Font.Black }
                Text { text: "TERMINAL"; color: "#3978F6"; font.pixelSize: 10; font.weight: Font.Bold }
                Rectangle { width: 1; height: 18; color: "#C6D5E0" }
                Text {
                    Layout.fillWidth: true
                    text: terminal.terminalTitle
                    color: "#33465A"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }

                Button {
                    text: "Copy"
                    enabled: terminal.running
                    implicitHeight: 32
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: "#24374B"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: terminal.copySelection()
                }
                Button {
                    text: "Paste"
                    enabled: terminal.running
                    implicitHeight: 32
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: "#24374B"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: terminal.pasteClipboard()
                }
                Button {
                    text: "Bottom"
                    enabled: terminal.scrollOffset > 0
                    implicitHeight: 32
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: parent.enabled ? "#24374B" : "#92A0AC"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: terminal.scrollToBottom()
                }
                Button {
                    text: "-"
                    implicitWidth: 34
                    implicitHeight: 32
                    ToolTip.visible: hovered
                    ToolTip.text: "Minimize MOKO Terminal"
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: "#24374B"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: window.showMinimized()
                }
                Button {
                    text: window.visibility === Window.Maximized ? "[]" : "[ ]"
                    implicitWidth: 38
                    implicitHeight: 32
                    ToolTip.visible: hovered
                    ToolTip.text: window.visibility === Window.Maximized ? "Restore MOKO Terminal" : "Maximize MOKO Terminal"
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: "#24374B"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: window.visibility = window.visibility === Window.Maximized
                                                   ? Window.Windowed : Window.Maximized
                }
                Button {
                    text: "X"
                    implicitWidth: 34
                    implicitHeight: 32
                    ToolTip.visible: hovered
                    ToolTip.text: "Close MOKO Terminal"
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "#FFFFFF"; border.color: "#C6D7E5" }
                    contentItem: Text { text: parent.text; color: "#24374B"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: window.close()
                }
            }
        }

        TerminalView {
            id: terminal
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            Component.onCompleted: {
                forceActiveFocus()
                startShell()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#20262D"
            border.color: "#303943"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text {
                    Layout.fillWidth: true
                    text: terminal.statusMessage
                    color: "#AFC0CF"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
                Text {
                    text: terminal.scrollbackLines + " scrollback lines"
                    color: "#8193A3"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 9
                }
            }
        }
    }

    MouseArea {
        width: 14
        height: 14
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        cursorShape: Qt.SizeFDiagCursor
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }

    Shortcut { sequence: "Ctrl+Shift+C"; onActivated: terminal.copySelection() }
    Shortcut { sequence: "Ctrl+Shift+V"; onActivated: terminal.pasteClipboard() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
                                       ? Window.Windowed : Window.FullScreen
    }
}
