import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: height < 640

    width: Math.min(340, parent ? parent.width * .24 : 340)
    height: Math.min(690, parent ? parent.height - (parent.height <= 760 ? 164 : 130) : 690)
    glassOpacity: .76
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.condensed ? 14 : 18
        spacing: root.condensed ? 8 : 14

        RowLayout {
            Layout.fillWidth: true
            Text { text: "MOKO"; color: "#12161C"; font.bold: true; font.pixelSize: 20 }
            Text { text: "AI"; color: "#455467"; font.pixelSize: 12 }
            Item { Layout.fillWidth: true }
            Text { text: "↗    ×"; color: "#455467"; font.pixelSize: 14 }
        }

        Item { Layout.preferredHeight: root.condensed ? 2 : 12 }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: root.condensed ? 64 : 88
            height: width
            radius: width / 2
            color: Qt.rgba(1,1,1,.35)
            border.color: Qt.rgba(1,1,1,.9)
            MokoGlyph { anchors.fill: parent; anchors.margins: root.condensed ? 13 : 18; kind: "ai" }
        }

        Text { Layout.alignment: Qt.AlignHCenter; text: "<font color='#3F7CFF'>Hello,</font> MOKO."; textFormat: Text.RichText; color: "#12161C"; font.pixelSize: root.condensed ? 22 : 26 }
        Text { Layout.alignment: Qt.AlignHCenter; text: "How can I assist you today?"; color: "#5D6875"; font.pixelSize: 12 }

        Repeater {
            model: [
                ["System Overview", "Get real-time status of your system.", "ai"],
                ["Optimize Performance", "Run smart optimization.", "settings"],
                ["Search Anything", "Search files, apps, and the web.", "browser"],
                ["Create Note", "Quickly write down your thoughts.", "calendar"]
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: root.condensed ? 48 : 64
                radius: root.condensed ? 12 : 14
                color: Qt.rgba(1,1,1,.55)
                Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: root.condensed ? 7 : 12; text: modelData[0]; color: "#2B3541"; font.pixelSize: 12; font.bold: true }
                Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: root.condensed ? 25 : 32; text: modelData[1]; color: "#7A8795"; font.pixelSize: 9 }
                MokoGlyph { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: root.condensed ? 24 : 28; height: width; kind: modelData[2] }
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.condensed ? 38 : 44
            radius: 13
            color: Qt.rgba(1,1,1,.52)
            border.color: Qt.rgba(.4,.55,.7,.13)
            Text { anchors.left: parent.left; anchors.leftMargin: 13; anchors.verticalCenter: parent.verticalCenter; text: "Ask MOKO AI…"; color: "#7D8996"; font.pixelSize: 11 }
            Text { anchors.right: parent.right; anchors.rightMargin: 13; anchors.verticalCenter: parent.verticalCenter; text: "➤"; color: "#3F7CFF"; font.pixelSize: 15 }
        }
        Text { Layout.alignment: Qt.AlignHCenter; text: "Developer Preview • AI actions are not connected yet"; color: "#94A0AE"; font.pixelSize: root.condensed ? 7 : 8 }
    }
}
