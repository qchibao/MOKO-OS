import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 1100
    height: 720
    minimumWidth: 820
    minimumHeight: 560
    title: "MOKO Files"
    color: "#EAF4FC"
    flags: Qt.Window | Qt.FramelessWindowHint

    property int selectedIndex: -1
    property string deleteToken: ""
    property var selectedEntry: selectedIndex >= 0 ? mokoFiles.entry(selectedIndex) : ({})
    property var propertyRows: []

    function resetSelection() {
        selectedIndex = -1
        fileList.currentIndex = -1
    }

    component CommandButton: Button {
        id: command
        property string hint: text
        implicitHeight: 36
        leftPadding: 13
        rightPadding: 13
        enabled: true
        font.pixelSize: 12
        background: Rectangle {
            radius: 7
            color: command.down ? "#D7E8FA" : command.hovered ? "#EFF7FF" : "#FFFFFF"
            border.color: command.enabled ? "#C7D9EA" : "#DCE5ED"
        }
        contentItem: Text {
            text: command.text
            color: command.enabled ? "#172334" : "#91A0AF"
            font: command.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        ToolTip.visible: hovered
        ToolTip.text: hint
        ToolTip.delay: 500
    }

    component DialogField: TextField {
        implicitHeight: 42
        leftPadding: 12
        rightPadding: 12
        selectByMouse: true
        color: "#1D2C3D"
        placeholderTextColor: "#8997A5"
        background: Rectangle {
            radius: 7
            color: "#FFFFFF"
            border.color: parent.activeFocus ? "#5C91FF" : "#C7D9EA"
        }
    }

    component MokoDialog: Dialog {
        id: mokoDialog
        property string primaryText: "OK"
        property string secondaryText: "Cancel"
        property bool destructive: false
        property int dialogWidth: 420
        width: Math.min(dialogWidth, window.width - 48)
        modal: true
        anchors.centerIn: parent
        padding: 20
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: 8
            color: "#F8FBFE"
            border.width: 1
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
                CommandButton {
                    visible: mokoDialog.secondaryText.length > 0
                    text: mokoDialog.secondaryText
                    onClicked: mokoDialog.reject()
                }
                Button {
                    text: mokoDialog.primaryText
                    implicitHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    background: Rectangle {
                        radius: 7
                        color: parent.down
                               ? (mokoDialog.destructive ? "#9D3541" : "#285FC4")
                               : parent.hovered
                                 ? (mokoDialog.destructive ? "#CB4A58" : "#4A84ED")
                                 : (mokoDialog.destructive ? "#B83E4B" : "#3978F6")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: mokoDialog.accept()
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#F8FBFE"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: "#F4FAFF"
            border.color: "#D5E3EF"

            MouseArea {
                anchors.fill: parent
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 8

                Text {
                    text: "MOKO"
                    color: "#10151C"
                    font.pixelSize: 21
                    font.weight: Font.Black
                }
                Text {
                    text: "FILES"
                    color: "#3978F6"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Item { Layout.preferredWidth: 14 }
                CommandButton {
                    text: "<"
                    hint: "Back"
                    enabled: mokoFiles.canGoBack
                    onClicked: { mokoFiles.goBack(); window.resetSelection() }
                }
                CommandButton {
                    text: ">"
                    hint: "Forward"
                    enabled: mokoFiles.canGoForward
                    onClicked: { mokoFiles.goForward(); window.resetSelection() }
                }
                CommandButton {
                    text: "Up"
                    hint: "Parent folder"
                    onClicked: { mokoFiles.navigateUp(); window.resetSelection() }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 7
                    color: "#FFFFFF"
                    border.color: pathInput.activeFocus ? "#5C91FF" : "#C7D9EA"

                    TextInput {
                        id: pathInput
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: TextInput.AlignVCenter
                        text: mokoFiles.currentPath
                        color: "#273445"
                        font.pixelSize: 12
                        selectByMouse: true
                        clip: true
                        onAccepted: {
                            if (mokoFiles.navigateTo(text))
                                window.resetSelection()
                            text = Qt.binding(function() { return mokoFiles.currentPath })
                        }
                    }
                }
                CommandButton { text: "Refresh"; onClicked: mokoFiles.refresh() }
                CommandButton {
                    text: "-"
                    hint: "Minimize MOKO Files"
                    onClicked: window.showMinimized()
                }
                CommandButton {
                    text: window.visibility === Window.Maximized ? "Restore" : "Maximize"
                    hint: text + " MOKO Files"
                    onClicked: window.visibility = window.visibility === Window.Maximized
                                                   ? Window.Windowed : Window.Maximized
                }
                CommandButton { text: "X"; hint: "Close MOKO Files"; onClicked: window.close() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                color: "#EDF6FD"
                border.color: "#D5E3EF"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        text: "PLACES"
                        color: "#667789"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Home"
                        leftPadding: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#172334"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 7
                            color: parent.hovered ? "#DCEEFF" : "#FFFFFF"
                            border.color: "#CADDEC"
                        }
                        onClicked: { mokoFiles.navigateHome(); window.resetSelection() }
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "File System"
                        leftPadding: 14
                        contentItem: Text {
                            text: parent.text
                            color: "#172334"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 7
                            color: parent.hovered ? "#DCEEFF" : "transparent"
                        }
                        onClicked: { mokoFiles.navigateTo("/"); window.resetSelection() }
                    }
                    Item { Layout.fillHeight: true }
                    Text {
                        Layout.fillWidth: true
                        text: "Operations use your current account permissions."
                        wrapMode: Text.Wrap
                        color: "#728294"
                        font.pixelSize: 10
                        lineHeight: 1.25
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    color: "#FFFFFF"
                    border.color: "#E0E9F0"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        spacing: 7

                        Text {
                            Layout.fillWidth: true
                            text: mokoFiles.displayPath
                            color: "#172334"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            elide: Text.ElideMiddle
                        }
                        CommandButton { text: "New Folder"; onClicked: { folderName.text = ""; newFolderDialog.open() } }
                        CommandButton { text: "Rename"; enabled: window.selectedIndex >= 0; onClicked: { renameName.text = window.selectedEntry.name || ""; renameDialog.open() } }
                        CommandButton { text: "Copy"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.stageCopy(window.selectedIndex) }
                        CommandButton { text: "Move"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.stageMove(window.selectedIndex) }
                        CommandButton { text: mokoFiles.canPaste ? "Paste " + mokoFiles.clipboardMode : "Paste"; enabled: mokoFiles.canPaste; onClicked: mokoFiles.paste() }
                        CommandButton { text: "Properties"; enabled: window.selectedIndex >= 0; onClicked: { window.propertyRows = mokoFiles.propertiesFor(window.selectedIndex); propertiesDialog.open() } }
                        CommandButton { text: "Delete"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.requestDelete(window.selectedIndex) }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: "#F5F9FC"
                    border.color: "#E0E9F0"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 52
                        anchors.rightMargin: 18
                        spacing: 12
                        Text { Layout.fillWidth: true; text: "Name"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text { Layout.preferredWidth: 130; text: "Type / Size"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text { Layout.preferredWidth: 145; text: "Modified"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
                    }
                }

                ListView {
                    id: fileList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: mokoFiles
                    clip: true
                    currentIndex: -1
                    boundsBehavior: Flickable.StopAtBounds
                    keyNavigationEnabled: true

                    delegate: Rectangle {
                        id: rowItem
                        required property int index
                        required property string name
                        required property string typeName
                        required property string sizeText
                        required property string modifiedText
                        required property bool isDirectory
                        width: fileList.width
                        height: 46
                        color: fileList.currentIndex === index ? "#DCEBFF" : mouse.containsMouse ? "#F2F8FD" : "#FFFFFF"
                        border.color: "#EDF2F6"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 18
                            spacing: 12

                            Rectangle {
                                width: 24
                                height: 20
                                radius: 4
                                color: rowItem.isDirectory ? "#5E94FF" : "#EEF3F8"
                                border.color: rowItem.isDirectory ? "#3F7CFF" : "#B9C8D5"
                                Text {
                                    anchors.centerIn: parent
                                    text: rowItem.isDirectory ? "" : "F"
                                    color: "#68798A"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: rowItem.name
                                color: "#172334"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 130
                                text: rowItem.isDirectory ? "Folder" : rowItem.sizeText
                                color: "#68798A"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 145
                                text: rowItem.modifiedText
                                color: "#68798A"
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                fileList.currentIndex = rowItem.index
                                window.selectedIndex = rowItem.index
                                fileList.forceActiveFocus()
                            }
                            onDoubleClicked: {
                                fileList.currentIndex = rowItem.index
                                window.selectedIndex = rowItem.index
                                if (mokoFiles.openEntry(rowItem.index))
                                    window.resetSelection()
                            }
                        }
                    }

                    Keys.onReturnPressed: function(event) {
                        if (currentIndex >= 0 && mokoFiles.openEntry(currentIndex))
                            window.resetSelection()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        if (currentIndex >= 0 && mokoFiles.openEntry(currentIndex))
                            window.resetSelection()
                        event.accepted = true
                    }
                    Keys.onDeletePressed: function(event) {
                        if (currentIndex >= 0)
                            mokoFiles.requestDelete(currentIndex)
                        event.accepted = true
                    }
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Backspace) {
                            mokoFiles.navigateUp()
                            window.resetSelection()
                            event.accepted = true
                        } else if (event.key === Qt.Key_F2 && currentIndex >= 0) {
                            renameName.text = window.selectedEntry.name || ""
                            renameDialog.open()
                            event.accepted = true
                        }
                    }

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    Text {
                        visible: fileList.count === 0
                        anchors.centerIn: parent
                        text: "This folder is empty"
                        color: "#748597"
                        font.pixelSize: 14
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#F5F9FC"
                    border.color: "#DDE7EF"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        Text { Layout.fillWidth: true; text: mokoFiles.statusMessage; color: "#657587"; font.pixelSize: 10; elide: Text.ElideRight }
                        Text { text: mokoFiles.count + (mokoFiles.count === 1 ? " item" : " items"); color: "#657587"; font.pixelSize: 10 }
                    }
                }
            }
        }
    }

    Connections {
        target: mokoFiles
        function onDeleteConfirmationRequested(name, path, token) {
            window.deleteToken = token
            deleteText.text = "Delete '" + name + "'?\n\nThis cannot be undone."
            deleteDialog.open()
        }
        function onErrorOccurred(message) {
            errorText.text = message
            errorDialog.open()
        }
    }

    MokoDialog {
        id: newFolderDialog
        title: "Create Folder"
        primaryText: "Create"
        onAccepted: mokoFiles.createFolder(folderName.text)
        DialogField {
            id: folderName
            width: 360
            placeholderText: "Folder name"
            onAccepted: newFolderDialog.accept()
        }
        onOpened: folderName.forceActiveFocus()
    }

    MokoDialog {
        id: renameDialog
        title: "Rename Item"
        primaryText: "Rename"
        onAccepted: mokoFiles.renameEntry(window.selectedIndex, renameName.text)
        DialogField {
            id: renameName
            width: 360
            onAccepted: renameDialog.accept()
        }
        onOpened: { renameName.forceActiveFocus(); renameName.selectAll() }
    }

    MokoDialog {
        id: deleteDialog
        title: "Confirm Delete"
        primaryText: "Delete"
        destructive: true
        onAccepted: { mokoFiles.confirmDelete(window.deleteToken); window.resetSelection() }
        onRejected: mokoFiles.cancelDelete()
        Text { id: deleteText; width: 360; wrapMode: Text.Wrap; color: "#263444"; font.pixelSize: 12 }
    }

    MokoDialog {
        id: propertiesDialog
        title: "Properties"
        primaryText: "Close"
        secondaryText: ""
        dialogWidth: 500
        contentItem: ColumnLayout {
            width: 430
            Repeater {
                model: window.propertyRows
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Text { Layout.preferredWidth: 105; text: modelData.label; color: "#68798A"; font.pixelSize: 11 }
                    Text { Layout.fillWidth: true; text: modelData.value; color: "#172334"; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                }
            }
        }
    }

    MokoDialog {
        id: errorDialog
        title: "MOKO Files"
        primaryText: "Close"
        secondaryText: ""
        Text { id: errorText; width: 360; wrapMode: Text.Wrap; color: "#263444"; font.pixelSize: 12 }
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

    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
                                       ? Window.Windowed : Window.FullScreen
    }
}
