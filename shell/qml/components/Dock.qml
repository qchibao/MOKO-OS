import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: root
    readonly property bool condensed: parent ? parent.width <= 1366 : false
    readonly property int buttonExtent: condensed ? 44 : 62
    readonly property int buttonSpacing: condensed ? 4 : 9
    readonly property int appCount: applicationModel ? applicationModel.count : 0
    property var applicationModel

    height: condensed ? 70 : 86
    width: Math.max(condensed ? 170 : 220,
                    28 + (appCount + 2) * buttonExtent + (appCount + 1) * buttonSpacing)
    radius: condensed ? 22 : 26
    glassOpacity: .70

    signal launcherRequested()
    signal aiRequested()
    signal appRequested(string appId)

    RowLayout {
        anchors.centerIn: parent
        spacing: root.buttonSpacing

        Rectangle {
            width: root.buttonExtent
            height: width
            radius: root.condensed ? 12 : 16
            color: launcherMouse.containsMouse ? Qt.rgba(1,1,1,.96) : Qt.rgba(1,1,1,.82)
            Text { anchors.centerIn: parent; text: "MOKO"; color: "#11151A"; font.bold: true; font.pixelSize: root.condensed ? 11 : 15 }
            MouseArea {
                id: launcherMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.launcherRequested()
            }
        }

        Repeater {
            model: root.applicationModel
            delegate: Item {
                id: dockDelegate
                required property string appId
                required property string displayName
                required property string iconSource
                required property string glyph
                required property string launchState
                required property string launchMessage
                width: root.buttonExtent
                height: root.buttonExtent

                AppTile {
                    anchors.centerIn: parent
                    compact: true
                    compactExtent: root.buttonExtent
                    title: dockDelegate.displayName
                    kind: dockDelegate.glyph
                    iconSource: dockDelegate.iconSource
                    launchState: dockDelegate.launchState
                    launchMessage: dockDelegate.launchMessage
                    onActivated: root.appRequested(dockDelegate.appId)
                }
            }
        }

        AppTile {
            compact: true
            compactExtent: root.buttonExtent
            title: "MOKO AI"
            kind: "ai"
            onActivated: root.aiRequested()
        }
    }
}
