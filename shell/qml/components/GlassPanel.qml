import QtQuick

Rectangle {
    id: root
    property real glassOpacity: 0.72
    property color glassColor: "#F8FCFF"
    readonly property string appearanceMode: typeof mokoDesktopSettings !== "undefined"
                                            ? mokoDesktopSettings.appearanceMode : "light"
    readonly property bool safeGraphics: typeof mokoDesktopSettings !== "undefined"
                                        && mokoDesktopSettings.safeGraphics
    readonly property bool glassEffectsEnabled: typeof mokoDesktopSettings !== "undefined"
                                                ? mokoDesktopSettings.glassEffectsEnabled : true

    radius: 24
    color: appearanceMode === "dark"
           ? Qt.rgba(.06, .10, .16, .94)
           : appearanceMode === "glass" && glassEffectsEnabled
             ? Qt.rgba(glassColor.r, glassColor.g, glassColor.b, glassOpacity)
             : Qt.rgba(glassColor.r, glassColor.g, glassColor.b, .98)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.82)

    layer.enabled: appearanceMode === "glass" && glassEffectsEnabled

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, parent.radius - 1)
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(0.45, 0.65, 0.85, 0.12)
    }
}
