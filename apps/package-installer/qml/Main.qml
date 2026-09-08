import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 620
    height: 560
    minimumWidth: 480
    minimumHeight: 440
    title: "MOKO Package Installer"
    color: "#F7FBFE"
    flags: Qt.Window | Qt.FramelessWindowHint

    onActiveChanged: {
        if (active && !confirmDialog.visible && mokoPackageInstaller.installable)
            Qt.callLater(installButton.forceActiveFocus)
    }

    function stateTitle() {
        if (mokoPackageInstaller.state === "installed") return "Installed"
        if (mokoPackageInstaller.state === "unsupported") return "Unsupported package"
        if (mokoPackageInstaller.state === "failed") return "Could not install package"
        if (mokoPackageInstaller.state === "installing") return "Installing package"
        if (mokoPackageInstaller.state === "cancelled") return "Installation cancelled"
        return "Review package"
    }

    function activatePrimaryAction() {
        if (failureDialog.visible)
            failureDialog.accept()
        else if (confirmDialog.visible)
            confirmDialog.accept()
        else if (mokoPackageInstaller.installable)
            confirmDialog.open()
    }

    component MokoDialog: Dialog {
        id: mokoDialog
        property string primaryText: "OK"
        property string secondaryText: "Cancel"
        property int dialogWidth: 460
        width: Math.min(dialogWidth, window.width - 40)
        modal: true
        anchors.centerIn: parent
        padding: 20
        closePolicy: Popup.CloseOnEscape
        onOpened: Qt.callLater(primaryButton.forceActiveFocus)
        background: Rectangle {
            radius: 8
            color: "#F8FBFE"
            border.color: "#BFD2E0"
        }
        header: Rectangle {
            implicitHeight: 54
            color: "#EAF4FC"
            border.color: "#D2E1EC"
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: mokoDialog.title
                color: "#152437"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
        }
        footer: Rectangle {
            implicitHeight: 62
            color: "#F3F8FC"
            border.color: "#D7E3EC"
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Button {
                    visible: mokoDialog.secondaryText.length > 0
                    text: mokoDialog.secondaryText
                    implicitHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    onClicked: mokoDialog.reject()
                    background: Rectangle { radius: 7; color: parent.hovered ? "#EFF6FD" : "#FFFFFF"; border.color: "#C9D9E6" }
                    contentItem: Text { text: parent.text; color: "#24384C"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    id: primaryButton
                    text: mokoDialog.primaryText
                    implicitHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    onClicked: mokoDialog.accept()
                    background: Rectangle { radius: 7; color: parent.down ? "#285FC4" : parent.hovered ? "#4A84ED" : "#3978F6" }
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }

    Rectangle { anchors.fill: parent; color: "#F7FBFE" }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#EFF7FD"
            border.color: "#D3E2ED"
            MouseArea { anchors.fill: parent; onPressed: window.startSystemMove(); onDoubleClicked: window.visibility = window.visibility === Window.Maximized ? Window.Windowed : Window.Maximized }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                Text { text: "MOKO"; color: "#111923"; font.pixelSize: 20; font.weight: Font.Black }
                Text { text: "PACKAGE INSTALLER"; color: "#3978F6"; font.pixelSize: 10; font.weight: Font.Bold }
                Item { Layout.fillWidth: true }
                Button {
                    text: "-"
                    implicitWidth: 34
                    implicitHeight: 30
                    onClicked: window.showMinimized()
                    background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "transparent" }
                    contentItem: Text {
                        text: parent.text
                        color: "#2B4055"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: "X"
                    implicitWidth: 34
                    implicitHeight: 30
                    onClicked: window.close()
                    background: Rectangle { radius: 6; color: parent.hovered ? "#F0DDE0" : "transparent" }
                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? "#A53D4B" : "#2B4055"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight + 36
            clip: true
            ColumnLayout {
                id: contentColumn
                width: parent.width
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.topMargin: 22
                spacing: 14
                Text { Layout.fillWidth: true; text: stateTitle(); color: "#142437"; font.pixelSize: 26; font.weight: Font.Bold }
                Text { Layout.fillWidth: true; text: mokoPackageInstaller.statusMessage; color: "#667A8D"; font.pixelSize: 12; wrapMode: Text.Wrap }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 230
                    radius: 8
                    color: "#FFFFFF"
                    border.color: "#D9E5EE"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Package"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: mokoPackageInstaller.packageName || "Unknown"; color: "#172334"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Version"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: mokoPackageInstaller.version || "-"; color: "#26384C"; font.pixelSize: 12 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Architecture"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: mokoPackageInstaller.architecture || "-"; color: "#26384C"; font.pixelSize: 12 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Installed size"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: mokoPackageInstaller.installedSize || "-"; color: "#26384C"; font.pixelSize: 12 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "File size"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: mokoPackageInstaller.fileSize || "-"; color: "#26384C"; font.pixelSize: 12 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Publisher"; color: "#718294"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { Layout.fillWidth: true; text: mokoPackageInstaller.maintainer || "-"; color: "#26384C"; font.pixelSize: 12; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                        }
                        Text { Layout.fillWidth: true; visible: mokoPackageInstaller.description.length > 0; text: mokoPackageInstaller.description; color: "#4D6175"; font.pixelSize: 11; wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 76
                    radius: 8
                    color: mokoPackageInstaller.packageType === "deb" ? "#FFF9E9" : "#F1F5F8"
                    border.color: mokoPackageInstaller.packageType === "deb" ? "#EAD9A4" : "#D6E0E8"
                    Text { anchors.fill: parent; anchors.margins: 14; text: mokoPackageInstaller.trustMessage || "Select a local package to inspect it."; color: "#6C5A2A"; font.pixelSize: 11; wrapMode: Text.Wrap; verticalAlignment: Text.AlignVCenter }
                }
                Text { Layout.fillWidth: true; visible: mokoPackageInstaller.packageType === "deb"; text: "Live-session installs are temporary. MOKO never installs an operating system or changes partitions."; color: "#718294"; font.pixelSize: 10; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 9
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Cancel"
                        implicitHeight: 38
                        enabled: !mokoPackageInstaller.busy
                        onClicked: { mokoPackageInstaller.cancel(); window.close() }
                        background: Rectangle { radius: 7; color: parent.hovered ? "#EFF6FD" : "#FFFFFF"; border.color: "#C9D9E6" }
                        contentItem: Text { text: parent.text; color: "#24384C"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        id: installButton
                        text: mokoPackageInstaller.busy ? "Installing..." : "Install"
                        implicitHeight: 38
                        enabled: mokoPackageInstaller.installable
                        onClicked: confirmDialog.open()
                        background: Rectangle { radius: 7; color: parent.enabled ? (parent.hovered ? "#4A84ED" : "#3978F6") : "#C8D7E5" }
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }
    }

    MokoDialog {
        id: confirmDialog
        objectName: "confirmDialog"
        title: "Confirm installation"
        primaryText: "Install"
        onAccepted: mokoPackageInstaller.install()
        ColumnLayout {
            width: 400
            spacing: 10
            Text { Layout.fillWidth: true; text: "Install " + (mokoPackageInstaller.packageName || "this package") + "?"; color: "#172334"; font.pixelSize: 14; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; text: "MOKO will ask the system authorization service and pass only the inspected local package to its fixed Debian helper."; color: "#667A8D"; font.pixelSize: 11; wrapMode: Text.Wrap }
        }
    }

    Connections {
        target: mokoPackageInstaller
        function onInstallFinished(success, message) { if (!success) failureDialog.open() }
    }
    MokoDialog {
        id: failureDialog
        title: "Package installation"
        primaryText: "Close"
        secondaryText: ""
        dialogWidth: 500
        property bool detailsVisible: false
        onClosed: detailsVisible = false
        ColumnLayout {
            width: 440
            spacing: 10
            Text { Layout.fillWidth: true; text: mokoPackageInstaller.statusMessage; color: "#263444"; font.pixelSize: 12; wrapMode: Text.Wrap }
            Button {
                visible: mokoPackageInstaller.developerDetails.length > 0
                text: failureDialog.detailsVisible ? "Hide Developer Details" : "Developer Details"
                implicitHeight: 34
                onClicked: failureDialog.detailsVisible = !failureDialog.detailsVisible
                background: Rectangle { radius: 7; color: parent.hovered ? "#EAF3FB" : "#FFFFFF"; border.color: "#C7D9E6" }
                contentItem: Text { text: parent.text; color: "#2C4C69"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            ScrollView {
                visible: failureDialog.detailsVisible
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                TextArea {
                    text: mokoPackageInstaller.developerDetails
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: "#30465A"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 10
                    background: Rectangle { color: "#EDF3F7"; border.color: "#CFDCE5"; radius: 6 }
                }
            }
        }
    }
    MouseArea { width: 14; height: 14; anchors.right: parent.right; anchors.bottom: parent.bottom; z: 100; cursorShape: Qt.SizeFDiagCursor; onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge) }
    // Keep Enter usable even when Qt Quick Controls has not assigned focus to
    // the primary button yet (common during a Wayland window-map transition).
    Shortcut {
        sequence: "Return"
        context: Qt.ApplicationShortcut
        enabled: failureDialog.visible || confirmDialog.visible
                 || mokoPackageInstaller.installable
        onActivated: window.activatePrimaryAction()
    }
    Shortcut {
        sequence: "Enter"
        context: Qt.ApplicationShortcut
        enabled: failureDialog.visible || confirmDialog.visible
                 || mokoPackageInstaller.installable
        onActivated: window.activatePrimaryAction()
    }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Escape"; onActivated: window.close() }
}
