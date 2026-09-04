import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window
    visible: true
    width: 1600
    height: 900
    minimumWidth: 1280
    minimumHeight: 720
    title: "MOKO OS v0.1 Developer Preview"
    visibility: Window.FullScreen
    color: "#E8F4FF"

    property bool launcherVisible: true
    property bool aiVisible: true
    property string clockText: ""

    function updateClock() {
        const d = new Date()
        clockText = Qt.formatTime(d, "hh:mm AP")
    }

    Component.onCompleted: updateClock()
    Timer { interval: 1000; repeat: true; running: true; onTriggered: window.updateClock() }

    Image {
        anchors.fill: parent
        source: "qrc:/qt/qml/MokoShell/assets/wallpapers/moko-ice-desktop.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(.93,.97,1,.08)
    }

    TopBar {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        clockText: window.clockText
        z: 10
    }

    LauncherPanel {
        id: launcher
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.top: topBar.bottom
        anchors.topMargin: 24
        visible: launcherVisible
        opacity: visible ? 1 : 0
        z: 4
        onAppRequested: (appId) => toast.show(appId + " is a Developer Preview tile")
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    AiPanel {
        id: aiPanel
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.top: topBar.bottom
        anchors.topMargin: 24
        visible: aiVisible
        opacity: visible ? 1 : 0
        z: 4
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 10
        spacing: -4
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "MOKO"
            color: "#0E1116"
            font.pixelSize: Math.min(138, window.width * .085)
            font.weight: Font.Black
            font.letterSpacing: -4
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "OS"
            color: "#4C82FF"
            font.pixelSize: 38
            font.letterSpacing: 7
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "v0.1  •  DEVELOPER PREVIEW"
            color: "#657487"
            font.pixelSize: 10
            font.letterSpacing: 1.4
        }
    }

    Dock {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        z: 7
        onLauncherRequested: window.launcherVisible = !window.launcherVisible
        onAiRequested: window.aiVisible = !window.aiVisible
    }

    Rectangle {
        id: toast
        property alias message: toastText.text
        function show(text) { message = text; visible = true; hideTimer.restart() }
        visible: false
        z: 20
        width: Math.min(420, toastText.implicitWidth + 40)
        height: 44
        radius: 16
        color: Qt.rgba(.08,.12,.18,.86)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 116
        Text { id: toastText; anchors.centerIn: parent; color: "white"; font.pixelSize: 12 }
        Timer { id: hideTimer; interval: 1800; onTriggered: toast.visible = false }
    }

    Shortcut { sequence: "Meta+Space"; onActivated: launcherVisible = !launcherVisible }
    Shortcut { sequence: "Meta+A"; onActivated: aiVisible = !aiVisible }
    Shortcut { sequence: "Escape"; onActivated: { launcherVisible = false; aiVisible = false } }
}
