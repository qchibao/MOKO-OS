import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: parent ? parent.width <= 1366 : false

    height: condensed ? 70 : 86
    width: condensed ? 490 : Math.min(950, parent ? parent.width * .63 : 950)
    radius: condensed ? 22 : 26
    glassOpacity: .70

    signal launcherRequested()
    signal aiRequested()

    RowLayout {
        anchors.centerIn: parent
        spacing: root.condensed ? 4 : 9

        Rectangle {
            width: root.condensed ? 44 : 62
            height: width
            radius: root.condensed ? 12 : 16
            color: Qt.rgba(1,1,1,.82)
            Text { anchors.centerIn: parent; text: "MOKO"; color: "#11151A"; font.bold: true; font.pixelSize: root.condensed ? 11 : 15 }
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
                compactExtent: root.condensed ? 44 : 62
                kind: modelData[0]
                onActivated: { if (modelData[0] === "ai") root.aiRequested() }
            }
        }
    }
}
