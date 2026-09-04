import QtQuick

Rectangle {
    id: root
    height: 40
    color: Qt.rgba(.94,.98,1,.66)
    border.color: Qt.rgba(1,1,1,.6)

    property string clockText: "--:--"

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 28

        Text { text: "MOKO"; color: "#11151A"; font.pixelSize: 18; font.bold: true; font.letterSpacing: 0 }
        Repeater {
            model: ["Desktop", "File", "Edit", "View", "Window", "Help"]
            Text { text: modelData; color: "#344151"; font.pixelSize: 13 }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 15
        Text { text: "☼"; color: "#344151"; font.pixelSize: 16 }
        Text { text: "⌁"; color: "#344151"; font.pixelSize: 18 }
        Text { text: "◖))"; color: "#344151"; font.pixelSize: 11 }
        Text { text: "100% ▰"; color: "#344151"; font.pixelSize: 11 }
        Text { text: root.clockText; color: "#263342"; font.pixelSize: 12 }
    }
}
