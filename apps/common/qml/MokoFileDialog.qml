import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    required property var picker
    property string mode: "open"
    property string dialogTitle: mode === "save" ? "Save File"
                                                  : mode === "folder" ? "Choose Folder" : "Open File"
    property string suggestedName: ""
    property int selectedIndex: -1
    property bool confirmOverwrite: false
    signal pathAccepted(string path)

    component DialogButton: Button {
        id: button
        property bool accent: false
        implicitWidth: Math.max(72, buttonText.implicitWidth + 24)
        implicitHeight: 34
        hoverEnabled: true
        contentItem: Text {
            id: buttonText
            text: button.text
            color: button.accent ? "#FFFFFF" : "#34516D"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 7
            color: !button.enabled ? "#E6EDF2"
                  : button.accent ? (button.down ? "#285FC8" : "#3978F6")
                  : button.down ? "#DCEAF4"
                  : button.hovered ? "#EDF5FA" : "#FFFFFF"
            border.color: button.accent ? "#2E67D6" : "#BCD0DE"
        }
    }

    function openAt(path) {
        selectedIndex = -1
        confirmOverwrite = false
        fileName.text = suggestedName
        picker.navigateTo(path && path.length > 0 ? path : picker.currentPath)
        open()
        Qt.callLater(function() {
            if (root.mode === "save")
                fileName.forceActiveFocus()
            else
                fileList.forceActiveFocus()
        })
    }

    function choose(forceOverwrite) {
        const path = picker.resolveSelection(mode, selectedIndex, fileName.text)
        if (!path || path.length === 0)
            return
        if (mode === "save" && picker.pathExists(path) && !forceOverwrite) {
            confirmOverwrite = true
            return
        }
        pathAccepted(path)
        close()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(720, parent ? parent.width - 40 : 720)
    height: Math.min(560, parent ? parent.height - 40 : 560)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        radius: 8
        color: "#F8FBFE"
        border.color: "#BFD3E2"
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#EAF4FC"
            border.color: "#C8DBE8"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: root.dialogTitle
                    color: "#172B3E"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                DialogButton {
                    text: "Home"
                    onClicked: { root.picker.navigateHome(); root.selectedIndex = -1 }
                }
                DialogButton {
                    text: "Up"
                    onClicked: { root.picker.navigateUp(); root.selectedIndex = -1 }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#FFFFFF"
            border.color: "#D9E5ED"
            Text {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                verticalAlignment: Text.AlignVCenter
                text: root.picker.displayPath
                color: "#4C6073"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.picker
            clip: true
            currentIndex: root.selectedIndex
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true
            activeFocusOnTab: true
            onCurrentIndexChanged: {
                if (activeFocus) {
                    root.selectedIndex = currentIndex
                    root.confirmOverwrite = false
                }
            }
            Keys.onReturnPressed: function(event) {
                const entry = root.picker.entry(root.selectedIndex)
                if (entry.isDirectory) {
                    root.picker.openDirectory(root.selectedIndex)
                    root.selectedIndex = -1
                } else if (root.mode === "open") {
                    root.choose(false)
                }
                event.accepted = true
            }
            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string path
                required property bool isDirectory
                required property string sizeText
                required property string modifiedText
                width: fileList.width
                height: 42
                color: root.selectedIndex === index ? "#DDEBFF"
                                                    : mouse.containsMouse ? "#EFF6FB" : "#FFFFFF"
                border.color: "#E6EDF2"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10
                    Rectangle {
                        width: 22
                        height: 18
                        radius: 4
                        color: row.isDirectory ? "#5A8FF1" : "#E6EDF3"
                        border.color: row.isDirectory ? "#3F76D8" : "#BAC9D5"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.name
                        color: "#203447"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 90
                        text: row.sizeText
                        color: "#718292"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 130
                        text: row.modifiedText
                        color: "#718292"
                        font.pixelSize: 10
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.selectedIndex = row.index
                        root.confirmOverwrite = false
                        if (root.mode === "save" && !row.isDirectory)
                            fileName.text = row.name
                    }
                    onDoubleClicked: {
                        if (row.isDirectory) {
                            root.picker.openDirectory(row.index)
                            root.selectedIndex = -1
                        } else if (root.mode === "open") {
                            root.selectedIndex = row.index
                            root.choose(false)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.mode === "save" ? 112 : 76
            color: "#F2F7FA"
            border.color: "#D5E2EB"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 7
                TextField {
                    id: fileName
                    visible: root.mode === "save"
                    Layout.fillWidth: true
                    placeholderText: "File name"
                    selectByMouse: true
                    onTextEdited: root.confirmOverwrite = false
                    onAccepted: root.choose(false)
                    color: "#203447"
                    placeholderTextColor: "#8998A5"
                    background: Rectangle {
                        radius: 7
                        color: "#FFFFFF"
                        border.color: fileName.activeFocus ? "#3978F6" : "#BCD0DE"
                        border.width: fileName.activeFocus ? 2 : 1
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.confirmOverwrite ? "A file already exists. Replace it?"
                                                    : root.picker.statusMessage
                        color: root.confirmOverwrite ? "#A24B53" : "#687B8D"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    DialogButton {
                        text: "Cancel"
                        onClicked: root.close()
                    }
                    DialogButton {
                        text: root.confirmOverwrite ? "Replace"
                                                     : root.mode === "save" ? "Save"
                                                     : root.mode === "folder" ? "Choose" : "Open"
                        accent: true
                        enabled: root.mode === "folder" || root.mode === "save"
                                 || (root.selectedIndex >= 0
                                     && !root.picker.entry(root.selectedIndex).isDirectory)
                        onClicked: root.choose(root.confirmOverwrite)
                    }
                }
            }
        }
    }
}
