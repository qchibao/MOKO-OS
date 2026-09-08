import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 1180
    height: 760
    minimumWidth: 860
    minimumHeight: 560
    title: "MOKO Files"
    color: "#F6FAFD"
    flags: Qt.Window | Qt.FramelessWindowHint

    property int selectedIndex: -1
    property string deleteToken: ""
    property var selectedEntry: selectedIndex >= 0 ? mokoFiles.entry(selectedIndex) : ({})
    property var propertyRows: []
    property bool gridView: false
    property bool detailsVisible: true

    function resetSelection() {
        selectedIndex = -1
        fileList.currentIndex = -1
        fileGrid.currentIndex = -1
    }

    function selectEntry(index) {
        selectedIndex = index
        fileList.currentIndex = index
        fileGrid.currentIndex = index
    }

    function openEntry(index) {
        selectEntry(index)
        if (mokoFiles.openEntry(index))
            resetSelection()
    }

    component CommandButton: Button {
        id: command
        property string hint: text
        implicitHeight: 34
        leftPadding: 12
        rightPadding: 12
        font.pixelSize: 11
        background: Rectangle {
            radius: 7
            color: command.down ? "#D6E7FA" : command.hovered ? "#EFF6FD" : "#FFFFFF"
            border.color: command.enabled ? "#C9D9E6" : "#DFE7ED"
        }
        contentItem: Text {
            text: command.text
            color: command.enabled ? "#1A2A3B" : "#98A5B2"
            font: command.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        ToolTip.visible: hovered
        ToolTip.text: hint
        ToolTip.delay: 450
    }

    component WindowButton: Button {
        id: control
        property string hint: ""
        implicitWidth: 36
        implicitHeight: 32
        padding: 0
        background: Rectangle {
            radius: 6
            color: control.down ? "#D6E7F7" : control.hovered ? "#E7F1FA" : "transparent"
        }
        contentItem: Text {
            text: control.text
            color: "#294057"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        ToolTip.visible: hovered
        ToolTip.text: hint
        ToolTip.delay: 450
    }

    component FileGlyph: Item {
        id: glyph
        property bool folder: false
        property string fileName: ""
        implicitWidth: 34
        implicitHeight: 30
        Rectangle {
            visible: glyph.folder
            x: 4
            y: 3
            width: 13
            height: 8
            radius: 3
            color: "#73A9FF"
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: glyph.folder ? 8 : 2
            width: 30
            height: glyph.folder ? 21 : 27
            radius: glyph.folder ? 5 : 4
            color: glyph.folder ? "#4A8CFF" : "#F2F6FA"
            border.color: glyph.folder ? "#3475E5" : "#BAC9D5"
            Text {
                visible: !glyph.folder
                anchors.centerIn: parent
                text: {
                    const parts = glyph.fileName.split(".")
                    return parts.length > 1 ? parts[parts.length - 1].slice(0, 3).toUpperCase() : "FILE"
                }
                color: "#66798C"
                font.pixelSize: 7
                font.weight: Font.Bold
            }
        }
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

    Rectangle { anchors.fill: parent; color: "#F7FBFE" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "#F2F8FD"
            border.color: "#D4E2ED"
            MouseArea {
                anchors.fill: parent
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 12
                spacing: 8
                Rectangle {
                    width: 22
                    height: 17
                    radius: 4
                    color: "#4A8CFF"
                    Rectangle { x: 3; y: -3; width: 10; height: 6; radius: 2; color: "#73A9FF" }
                }
                Text { text: "MOKO Files"; color: "#111923"; font.pixelSize: 15; font.weight: Font.Bold }
                Item { Layout.fillWidth: true }
                WindowButton { text: "-"; hint: "Minimize"; onClicked: window.showMinimized() }
                WindowButton {
                    text: window.visibility === Window.Maximized ? "o" : "[]"
                    hint: window.visibility === Window.Maximized ? "Restore" : "Maximize"
                    onClicked: window.visibility = window.visibility === Window.Maximized
                                                   ? Window.Windowed : Window.Maximized
                }
                WindowButton { text: "X"; hint: "Close"; onClicked: window.close() }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                SplitView.minimumWidth: 176
                SplitView.preferredWidth: 205
                SplitView.maximumWidth: 260
                color: "#EDF6FD"
                border.color: "#D5E3EF"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 13
                    spacing: 8
                    Text { text: "PLACES"; color: "#687A8C"; font.pixelSize: 10; font.weight: Font.Bold }
                    ListView {
                        id: placesList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: mokoFiles.places
                        clip: true
                        spacing: 3
                        delegate: Button {
                            required property var modelData
                            width: placesList.width
                            implicitHeight: 36
                            leftPadding: 10
                            rightPadding: 8
                            contentItem: RowLayout {
                                spacing: 8
                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 16
                                    radius: 3
                                    color: modelData.kind === "trash" ? "#A8B6C5" : "#5A96F8"
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.kind === "volume" ? "U" : modelData.kind === "network" ? "N" : ""
                                        color: "#FFFFFF"
                                        font.pixelSize: 7
                                        font.weight: Font.Bold
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: "#18283A"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            background: Rectangle {
                                radius: 7
                                color: mokoFiles.currentPath === modelData.path
                                       ? "#D7E8FF" : parent.hovered ? "#F5FAFE" : "transparent"
                                border.color: mokoFiles.currentPath === modelData.path
                                              ? "#B1CDF5" : "transparent"
                            }
                            onClicked: {
                                if (mokoFiles.navigateTo(modelData.path))
                                    window.resetSelection()
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Mounted USB and network locations appear here when available."
                        wrapMode: Text.Wrap
                        color: "#718294"
                        font.pixelSize: 9
                        lineHeight: 1.25
                    }
                }
            }

            ColumnLayout {
                SplitView.fillWidth: true
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    color: "#FFFFFF"
                    border.color: "#DFE8EF"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        anchors.rightMargin: 13
                        spacing: 7
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
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            Layout.preferredHeight: 36
                            radius: 7
                            color: "#F9FBFD"
                            border.color: "#D1DFE9"
                            clip: true
                            Flickable {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                contentWidth: breadcrumbRow.width
                                contentHeight: height
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                Row {
                                    id: breadcrumbRow
                                    height: parent.height
                                    spacing: 1
                                    Repeater {
                                        model: mokoFiles.breadcrumbs
                                        delegate: Row {
                                            required property int index
                                            required property var modelData
                                            height: breadcrumbRow.height
                                            spacing: 1
                                            Button {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: modelData.label
                                                implicitHeight: 28
                                                leftPadding: 7
                                                rightPadding: 7
                                                background: Rectangle { radius: 5; color: parent.hovered ? "#E7F1FA" : "transparent" }
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: "#31465A"
                                                    font.pixelSize: 11
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                onClicked: {
                                                    if (mokoFiles.navigateTo(modelData.path))
                                                        window.resetSelection()
                                                }
                                            }
                                            Text {
                                                visible: index < mokoFiles.breadcrumbs.length - 1
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: ">"
                                                color: "#91A1AF"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        TextField {
                            id: fileSearch
                            Layout.preferredWidth: Math.min(220, window.width * 0.2)
                            Layout.minimumWidth: 130
                            implicitHeight: 36
                            placeholderText: "Search in " + mokoFiles.displayPath
                            text: mokoFiles.searchText
                            selectByMouse: true
                            onTextEdited: mokoFiles.searchText = text
                            color: "#1D2C3D"
                            placeholderTextColor: "#8997A5"
                            background: Rectangle {
                                radius: 7
                                color: "#F9FBFD"
                                border.color: fileSearch.activeFocus ? "#5C91FF" : "#D1DFE9"
                            }
                        }
                        CommandButton { text: "List"; hint: "List view"; checkable: true; checked: !window.gridView; onClicked: window.gridView = false }
                        CommandButton { text: "Grid"; hint: "Grid view"; checkable: true; checked: window.gridView; onClicked: window.gridView = true }
                        CommandButton { text: "Info"; hint: "Show or hide details"; checkable: true; checked: window.detailsVisible; onClicked: window.detailsVisible = !window.detailsVisible }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: window.gridView ? 1 : 0

                        ColumnLayout {
                            spacing: 0
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                color: "#F6F9FC"
                                border.color: "#E1E9F0"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 54
                                    anchors.rightMargin: 16
                                    spacing: 12
                                    Text { Layout.fillWidth: true; text: "Name"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
                                    Text { Layout.preferredWidth: 135; text: "Kind / Size"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
                                    Text { Layout.preferredWidth: 142; text: "Modified"; color: "#657587"; font.pixelSize: 10; font.weight: Font.Bold }
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
                                    height: 45
                                    color: fileList.currentIndex === index ? "#DCEBFF"
                                           : rowMouse.containsMouse ? "#F2F8FD" : "#FFFFFF"
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#EEF3F6" }
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 16
                                        spacing: 10
                                        FileGlyph { folder: rowItem.isDirectory; fileName: rowItem.name }
                                        Text { Layout.fillWidth: true; text: rowItem.name; color: "#172334"; font.pixelSize: 12; elide: Text.ElideRight }
                                        Text {
                                            Layout.preferredWidth: 135
                                            text: rowItem.isDirectory ? "Folder" : rowItem.typeName + " - " + rowItem.sizeText
                                            color: "#68798A"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                        Text { Layout.preferredWidth: 142; text: rowItem.modifiedText; color: "#68798A"; font.pixelSize: 10; elide: Text.ElideRight }
                                    }
                                    MouseArea {
                                        id: rowMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: { window.selectEntry(rowItem.index); fileList.forceActiveFocus() }
                                        onDoubleClicked: window.openEntry(rowItem.index)
                                    }
                                }
                                Keys.onReturnPressed: function(event) { if (currentIndex >= 0) window.openEntry(currentIndex); event.accepted = true }
                                Keys.onEnterPressed: function(event) { if (currentIndex >= 0) window.openEntry(currentIndex); event.accepted = true }
                                Keys.onDeletePressed: function(event) { if (currentIndex >= 0) mokoFiles.requestDelete(currentIndex); event.accepted = true }
                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Backspace) {
                                        mokoFiles.navigateUp(); window.resetSelection(); event.accepted = true
                                    } else if (event.key === Qt.Key_F2 && currentIndex >= 0) {
                                        renameName.text = window.selectedEntry.name || ""; renameDialog.open(); event.accepted = true
                                    }
                                }
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                Text {
                                    visible: fileList.count === 0
                                    anchors.centerIn: parent
                                    text: mokoFiles.searchText.length ? "No matching files" : "This folder is empty"
                                    color: "#748597"
                                    font.pixelSize: 13
                                }
                            }
                        }

                        GridView {
                            id: fileGrid
                            model: mokoFiles
                            clip: true
                            currentIndex: -1
                            cellWidth: 132
                            cellHeight: 112
                            leftMargin: 12
                            topMargin: 12
                            keyNavigationEnabled: true
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Rectangle {
                                id: gridItem
                                required property int index
                                required property string name
                                required property string sizeText
                                required property bool isDirectory
                                width: 120
                                height: 98
                                radius: 7
                                color: fileGrid.currentIndex === index ? "#DCEBFF"
                                       : gridMouse.containsMouse ? "#F0F7FD" : "transparent"
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 5
                                    FileGlyph {
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.preferredWidth: 42
                                        Layout.preferredHeight: 38
                                        folder: gridItem.isDirectory
                                        fileName: gridItem.name
                                        scale: 1.2
                                    }
                                    Text { Layout.fillWidth: true; text: gridItem.name; color: "#172334"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                    Text { Layout.fillWidth: true; text: gridItem.isDirectory ? "Folder" : gridItem.sizeText; color: "#758597"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                }
                                MouseArea {
                                    id: gridMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: { window.selectEntry(gridItem.index); fileGrid.forceActiveFocus() }
                                    onDoubleClicked: window.openEntry(gridItem.index)
                                }
                            }
                            Keys.onReturnPressed: function(event) { if (currentIndex >= 0) window.openEntry(currentIndex); event.accepted = true }
                            Keys.onEnterPressed: function(event) { if (currentIndex >= 0) window.openEntry(currentIndex); event.accepted = true }
                            Keys.onDeletePressed: function(event) { if (currentIndex >= 0) mokoFiles.requestDelete(currentIndex); event.accepted = true }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            Text { visible: fileGrid.count === 0; anchors.centerIn: parent; text: mokoFiles.searchText.length ? "No matching files" : "This folder is empty"; color: "#748597"; font.pixelSize: 13 }
                        }
                    }

                    Rectangle {
                        visible: window.detailsVisible
                        Layout.preferredWidth: 218
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: window.detailsVisible ? 218 : 0
                        Layout.fillHeight: true
                        color: "#F3F8FC"
                        border.color: "#DDE7EF"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8
                            FileGlyph {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 14
                                Layout.preferredWidth: 68
                                Layout.preferredHeight: 62
                                folder: window.selectedEntry.isDirectory || false
                                fileName: window.selectedEntry.name || ""
                                scale: 1.8
                            }
                            Text {
                                Layout.fillWidth: true
                                text: window.selectedEntry.name || "Nothing selected"
                                color: "#142437"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: window.selectedIndex >= 0
                                      ? (window.selectedEntry.typeName || "File") + " - " + (window.selectedEntry.sizeText || "")
                                      : "Select an item to see its details."
                                color: "#687A8C"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#D9E4EC" }
                            Text { visible: window.selectedIndex >= 0; Layout.fillWidth: true; text: "Modified"; color: "#82909E"; font.pixelSize: 9 }
                            Text { visible: window.selectedIndex >= 0; Layout.fillWidth: true; text: window.selectedEntry.modifiedText || ""; color: "#26384B"; font.pixelSize: 10; wrapMode: Text.Wrap }
                            Text { visible: window.selectedIndex >= 0; Layout.fillWidth: true; text: "Location"; color: "#82909E"; font.pixelSize: 9 }
                            Text { visible: window.selectedIndex >= 0; Layout.fillWidth: true; text: mokoFiles.displayPath; color: "#26384B"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; maximumLineCount: 5; elide: Text.ElideMiddle }
                            Item { Layout.fillHeight: true }
                            CommandButton {
                                Layout.fillWidth: true
                                text: "Properties"
                                enabled: window.selectedIndex >= 0
                                onClicked: { window.propertyRows = mokoFiles.propertiesFor(window.selectedIndex); propertiesDialog.open() }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    color: "#FFFFFF"
                    border.color: "#DFE8EF"
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 7
                        CommandButton { text: "New Folder"; onClicked: { folderName.text = ""; newFolderDialog.open() } }
                        CommandButton { text: "Copy"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.stageCopy(window.selectedIndex) }
                        CommandButton { text: "Move"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.stageMove(window.selectedIndex) }
                        CommandButton { text: mokoFiles.canPaste ? "Paste " + mokoFiles.clipboardMode : "Paste"; enabled: mokoFiles.canPaste; onClicked: mokoFiles.paste() }
                        CommandButton { text: "Rename"; enabled: window.selectedIndex >= 0; onClicked: { renameName.text = window.selectedEntry.name || ""; renameDialog.open() } }
                        CommandButton { text: "Delete"; enabled: window.selectedIndex >= 0; onClicked: mokoFiles.requestDelete(window.selectedIndex) }
                        CommandButton { text: "Properties"; enabled: window.selectedIndex >= 0; onClicked: { window.propertyRows = mokoFiles.propertiesFor(window.selectedIndex); propertiesDialog.open() } }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "#F4F8FB"
                    border.color: "#DDE7EF"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        anchors.rightMargin: 13
                        Text { Layout.fillWidth: true; text: mokoFiles.statusMessage; color: "#657587"; font.pixelSize: 9; elide: Text.ElideRight }
                        Text { text: mokoFiles.count + (mokoFiles.count === 1 ? " item" : " items"); color: "#657587"; font.pixelSize: 9 }
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
        function onErrorOccurred(message) { errorText.text = message; errorDialog.open() }
        function onCountChanged() { if (window.selectedIndex >= mokoFiles.count) window.resetSelection() }
    }

    MokoDialog {
        id: newFolderDialog
        title: "Create Folder"
        primaryText: "Create"
        onAccepted: mokoFiles.createFolder(folderName.text)
        DialogField { id: folderName; width: 360; placeholderText: "Folder name"; onAccepted: newFolderDialog.accept() }
        onOpened: folderName.forceActiveFocus()
    }
    MokoDialog {
        id: renameDialog
        title: "Rename Item"
        primaryText: "Rename"
        onAccepted: mokoFiles.renameEntry(window.selectedIndex, renameName.text)
        DialogField { id: renameName; width: 360; onAccepted: renameDialog.accept() }
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
    Shortcut { sequence: "Ctrl+F"; onActivated: fileSearch.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+L"; onActivated: fileSearch.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+C"; enabled: window.selectedIndex >= 0; onActivated: mokoFiles.stageCopy(window.selectedIndex) }
    Shortcut { sequence: "Ctrl+X"; enabled: window.selectedIndex >= 0; onActivated: mokoFiles.stageMove(window.selectedIndex) }
    Shortcut { sequence: "Ctrl+V"; enabled: mokoFiles.canPaste; onActivated: mokoFiles.paste() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: { folderName.text = ""; newFolderDialog.open() } }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut { sequence: "F11"; onActivated: window.visibility = window.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
}
