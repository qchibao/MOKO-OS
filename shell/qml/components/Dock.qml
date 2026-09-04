import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: root
    height: 86
    width: Math.min(950, parent ? parent.width * .63 : 950)
    radius: 26
    glassOpacity: .70

    signal launcherRequested()
    signal aiRequested()

    RowLayout {
        anchors.centerIn: parent
        spacing: 9

        Rectangle {
            width: 62; height: 62; radius: 16
            color: Qt.rgba(1,1,1,.82)
            Text { anchors.centerIn: parent; text: "MOKO"; color: "#11151A"; font.bold: true; font.pixelSize: 15 }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.launcherRequested() }
        }
        Repeater {
            model: [
                ["files","Files"], ["browser","Browser"], ["ai","AI"], ["mail","Mail"], ["calendar","Calendar"],
                ["terminal","Terminal"], ["settings","Settings"], ["store","Store"], ["trash","Trash"]
            ]
            delegate: AppTile {
                required property var modelData
                compact: true
                kind: modelData[0]
                onActivated: { if (modelData[0] === "ai") root.aiRequested() }
            }
        }
    }
}
