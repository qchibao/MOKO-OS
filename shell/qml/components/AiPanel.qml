import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: height < 680
    property var controller

    width: Math.min(340, parent ? parent.width * .24 : 340)
    height: Math.min(690, parent ? parent.height - (parent.height <= 760 ? 164 : 130) : 690)
    glassOpacity: .76
    clip: true

    function focusInput() {
        Qt.callLater(function() {
            prompt.forceActiveFocus()
            prompt.selectAll()
        })
    }

    function submit(text) {
        if (!root.controller || !text.trim())
            return
        root.controller.submit(text)
        prompt.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.condensed ? 14 : 18
        spacing: root.condensed ? 7 : 10

        RowLayout {
            Layout.fillWidth: true
            Text { text: "MOKO"; color: "#12161C"; font.bold: true; font.pixelSize: 20 }
            Text { text: "AI"; color: "#455467"; font.pixelSize: 12 }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.controller && root.controller.connected
                       ? (root.controller.providerAvailable ? "#28A978" : "#D58A2C")
                       : "#C55B66"
            }
            Text {
                text: root.controller && root.controller.connected ? "Connected" : "Disconnected"
                color: "#455467"
                font.pixelSize: 9
            }
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
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.controller && root.controller.processing
                  ? "Working on your request..."
                  : root.controller ? root.controller.state : "Provider unavailable"
            color: "#5D6875"
            font.pixelSize: 12
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.condensed ? 62 : 72
            radius: 8
            color: root.controller && root.controller.failed
                   ? Qt.rgba(.86,.25,.30,.10) : Qt.rgba(1,1,1,.52)
            border.color: root.controller && root.controller.failed
                          ? Qt.rgba(.75,.20,.25,.25) : Qt.rgba(.4,.55,.7,.13)
            Text {
                anchors.fill: parent
                anchors.margins: 12
                text: root.controller ? root.controller.response : "MOKO AI daemon unavailable."
                color: root.controller && root.controller.failed ? "#9D3440" : "#334252"
                font.pixelSize: root.condensed ? 10 : 11
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                maximumLineCount: root.condensed ? 3 : 4
            }
        }

        Repeater {
            model: [
                ["System Overview", "Read live Linux system status.", "ai", "system overview"],
                ["Open MOKO Files", "Launch through the MOKO app registry.", "files", "open files"],
                ["Network Status", "Read NetworkManager state.", "browser", "network status"],
                ["Storage Status", "Read mounted filesystem capacity.", "settings", "storage status"]
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: root.condensed ? 46 : 56
                radius: 8
                color: actionMouse.containsMouse ? Qt.rgba(1,1,1,.78) : Qt.rgba(1,1,1,.55)
                Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: root.condensed ? 7 : 12; text: modelData[0]; color: "#2B3541"; font.pixelSize: 12; font.bold: true }
                Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: root.condensed ? 25 : 32; text: modelData[1]; color: "#7A8795"; font.pixelSize: 9 }
                MokoGlyph { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: root.condensed ? 24 : 28; height: width; kind: modelData[2] }
                MouseArea {
                    id: actionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: !root.controller || !root.controller.processing
                    onClicked: root.submit(modelData[3])
                }
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.condensed ? 38 : 44
            radius: 8
            color: Qt.rgba(1,1,1,.52)
            border.color: Qt.rgba(.4,.55,.7,.13)
            TextField {
                id: prompt
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 38
                placeholderText: "Ask MOKO AI..."
                color: "#263342"
                font.pixelSize: 11
                background: null
                enabled: root.controller && root.controller.connected
                         && root.controller.providerAvailable && !root.controller.processing
                onAccepted: root.submit(text)
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 13
                anchors.verticalCenter: parent.verticalCenter
                text: "➤"
                color: prompt.enabled && prompt.text.trim() ? "#3F7CFF" : "#9DADBE"
                font.pixelSize: 15
            }
            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 38
                cursorShape: Qt.PointingHandCursor
                enabled: prompt.enabled && prompt.text.trim()
                onClicked: root.submit(prompt.text)
            }
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "On-device actions only"
            color: "#8190A0"
            font.pixelSize: root.condensed ? 7 : 8
        }
    }
}
