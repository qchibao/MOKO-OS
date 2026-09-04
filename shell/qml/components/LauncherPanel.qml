import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: height < 640

    width: Math.min(510, parent ? parent.width * .33 : 510)
    height: Math.min(690, parent ? parent.height - (parent.height <= 760 ? 164 : 130) : 690)
    glassOpacity: .76
    clip: true

    signal appRequested(string appId)

    property var apps: [
        { title: "Files", kind: "files" },
        { title: "Browser", kind: "browser" },
        { title: "MOKO AI", kind: "ai" },
        { title: "Mail", kind: "mail" },
        { title: "Calendar", kind: "calendar" },
        { title: "Music", kind: "music" },
        { title: "Camera", kind: "camera" },
        { title: "Notes", kind: "calendar" },
        { title: "Terminal", kind: "terminal" },
        { title: "Settings", kind: "settings" },
        { title: "Store", kind: "store" },
        { title: "Security", kind: "security" },
        { title: "Cloud", kind: "cloud" },
        { title: "Trash", kind: "trash" }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.condensed ? 16 : 20
        spacing: root.condensed ? 8 : 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            radius: 13
            color: Qt.rgba(1,1,1,.62)
            border.color: Qt.rgba(.57,.68,.80,.18)

            TextInput {
                id: search
                anchors.fill: parent
                anchors.leftMargin: 38
                anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: "#19202A"
                font.pixelSize: 13
                clip: true
            }
            Text { anchors.left: parent.left; anchors.leftMargin: 13; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; font.pixelSize: 22; color: "#647284" }
            Text { visible: search.text.length === 0; anchors.left: search.left; anchors.verticalCenter: parent.verticalCenter; text: "Search apps, files, settings…"; color: "#7A8796"; font.pixelSize: 12 }
        }

        Text { text: "APPS"; color: "#4E5B69"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0 }

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            rowSpacing: root.condensed ? 2 : 3
            columnSpacing: Math.max(2, (width - columns * (root.condensed ? 68 : 76)) / (columns - 1))

            Repeater {
                model: root.apps
                delegate: AppTile {
                    required property var modelData
                    title: modelData.title
                    kind: modelData.kind
                    dense: root.condensed
                    visible: search.text.length === 0 || title.toLowerCase().includes(search.text.toLowerCase())
                    onActivated: root.appRequested(title)
                }
            }
        }

        Text { text: "RECENT FILES"; color: "#4E5B69"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0 }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.condensed ? 3 : 5
            Repeater {
                model: [
                    ["MOKO_OS_Press_Kit.pdf", "Documents", "2m ago"],
                    ["System_Overview.key", "Presentations", "1h ago"],
                    ["Brand_Guide_v0.1.pdf", "Documents", "3h ago"],
                    ["Wallpaper_Concept_01.png", "Images", "Yesterday"],
                    ["Release_Notes_v0.1.txt", "Documents", "Yesterday"]
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.condensed ? 24 : 31
                    radius: 9
                    color: hover.containsMouse ? Qt.rgba(1,1,1,.52) : "transparent"
                    Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "▣"; color: "#5A8DFF"; font.pixelSize: 14 }
                    Text { anchors.left: parent.left; anchors.leftMargin: 30; anchors.verticalCenter: parent.verticalCenter; text: modelData[0]; color: "#374555"; font.pixelSize: 10 }
                    Text { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData[2]; color: "#8793A1"; font.pixelSize: 9 }
                    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "♙  Lock"; color: "#4C5A69"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Text { text: "↻  Restart"; color: "#4C5A69"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Text { text: "⏻  Shut Down"; color: "#4C5A69"; font.pixelSize: 11 }
        }
    }
}
