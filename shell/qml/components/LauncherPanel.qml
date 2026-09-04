import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: height < 640
    property var applicationModel

    width: Math.min(510, parent ? parent.width * .33 : 510)
    height: Math.min(690, parent ? parent.height - (parent.height <= 760 ? 164 : 130) : 690)
    glassOpacity: .76
    clip: true

    signal appRequested(string appId)

    function focusSearch() {
        Qt.callLater(function() {
            search.forceActiveFocus()
            search.selectAll()
        })
    }

    function activateCurrent() {
        if (appGrid.count > 0 && appGrid.currentItem)
            root.appRequested(appGrid.currentItem.appId)
    }

    onVisibleChanged: {
        if (visible)
            focusSearch()
    }
    Component.onCompleted: {
        if (visible)
            focusSearch()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.condensed ? 16 : 20
        spacing: root.condensed ? 8 : 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            radius: 13
            color: Qt.rgba(1,1,1,.62)
            border.color: search.activeFocus ? "#78A7FF" : Qt.rgba(.57,.68,.80,.18)

            TextInput {
                id: search
                anchors.fill: parent
                anchors.leftMargin: 38
                anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: "#19202A"
                font.pixelSize: 13
                clip: true
                onTextChanged: {
                    if (root.applicationModel)
                        root.applicationModel.searchText = text
                    appGrid.currentIndex = appGrid.count > 0 ? 0 : -1
                }
                Keys.onDownPressed: function(event) {
                    if (appGrid.count > 0) {
                        appGrid.forceActiveFocus()
                        appGrid.currentIndex = Math.max(0, appGrid.currentIndex)
                        event.accepted = true
                    }
                }
                Keys.onReturnPressed: function(event) {
                    root.activateCurrent()
                    event.accepted = true
                }
                Keys.onEnterPressed: function(event) {
                    root.activateCurrent()
                    event.accepted = true
                }
                Keys.onEscapePressed: function(event) {
                    if (text.length > 0) {
                        clear()
                        event.accepted = true
                    }
                }
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 13
                anchors.verticalCenter: parent.verticalCenter
                text: "⌕"
                font.pixelSize: 22
                color: "#647284"
            }
            Text {
                visible: search.text.length === 0
                anchors.left: search.left
                anchors.verticalCenter: parent.verticalCenter
                text: "Search installed applications..."
                color: "#7A8796"
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "APPLICATIONS"; color: "#4E5B69"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0 }
            Item { Layout.fillWidth: true }
            Text {
                text: root.applicationModel ? root.applicationModel.count + " installed" : "0 installed"
                color: "#7A8796"
                font.pixelSize: 10
            }
        }

        GridView {
            id: appGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            model: root.applicationModel
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true
            keyNavigationWraps: true
            cellWidth: width / 5
            cellHeight: root.condensed ? 78 : 100
            currentIndex: count > 0 ? 0 : -1

            delegate: Item {
                id: appDelegate
                required property int index
                required property string appId
                required property string displayName
                required property string iconSource
                required property string glyph
                required property string category
                required property string description
                required property string launchState
                required property string launchMessage
                width: appGrid.cellWidth
                height: appGrid.cellHeight

                AppTile {
                    anchors.centerIn: parent
                    title: appDelegate.displayName
                    kind: appDelegate.glyph
                    iconSource: appDelegate.iconSource
                    launchState: appDelegate.launchState
                    launchMessage: appDelegate.launchMessage
                    dense: root.condensed
                    selected: appGrid.currentIndex === appDelegate.index
                    onActivated: {
                        appGrid.currentIndex = appDelegate.index
                        root.appRequested(appDelegate.appId)
                    }
                }
            }

            Keys.onReturnPressed: function(event) {
                root.activateCurrent()
                event.accepted = true
            }
            Keys.onEnterPressed: function(event) {
                root.activateCurrent()
                event.accepted = true
            }
            Keys.onEscapePressed: function(event) {
                search.forceActiveFocus()
                event.accepted = true
            }
            Keys.onPressed: function(event) {
                if (event.text && event.text.length === 1) {
                    search.forceActiveFocus()
                    search.insert(search.cursorPosition, event.text)
                    event.accepted = true
                }
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.condensed ? 50 : 58
            radius: 12
            color: Qt.rgba(1,1,1,.42)
            border.color: Qt.rgba(.57,.68,.80,.14)

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 3
                Text {
                    width: parent.width
                    text: appGrid.currentItem ? appGrid.currentItem.displayName : "No matching applications"
                    color: "#263342"
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: {
                        if (!appGrid.currentItem)
                            return "Try another app name, category, or keyword."
                        const state = appGrid.currentItem.launchState
                        if (state === "launching")
                            return "Launching..."
                        if (state === "running")
                            return appGrid.currentItem.launchMessage || "Running"
                        if (state === "failed")
                            return "Failed: " + appGrid.currentItem.launchMessage
                        return appGrid.currentItem.description || appGrid.currentItem.category
                    }
                    color: appGrid.currentItem && appGrid.currentItem.launchState === "failed" ? "#B53543" : "#718092"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
            }
        }
    }
}
