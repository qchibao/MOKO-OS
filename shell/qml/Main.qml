import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window
    readonly property bool previewMode: Qt.application.arguments.indexOf("--windowed") !== -1
                                        || Qt.application.arguments.indexOf("--screenshot") !== -1

    visible: true
    width: 1600
    height: 900
    minimumWidth: 1280
    minimumHeight: 720
    title: "MOKO OS v0.1.1 Hardware & Usability Preview"
    visibility: previewMode ? Window.Windowed : Window.FullScreen
    color: "#E8F4FF"

    property bool launcherVisible: true
    property bool aiVisible: true
    property bool controlCenterVisible: false
    property bool notificationCenterVisible: false
    property string activeSystemPanel: ""
    property alias controlCenterPage: controlCenter.currentPage
    property string clockText: ""
    property string dateText: ""

    function openApplication(appId) {
        activeSystemPanel = ""
        launcherVisible = false
        aiVisible = false
        controlCenterVisible = false
        notificationCenterVisible = false
        mokoWindowManager.setShellOverlay(false)
        if (!mokoWindowManager.activateApplication(appId))
            mokoApplicationRegistry.launch(appId)
    }

    function toggleSystemPanel(panel) {
        if (activeSystemPanel === panel) {
            activeSystemPanel = ""
            launcherVisible = false
            aiVisible = false
            controlCenterVisible = false
            notificationCenterVisible = false
            mokoWindowManager.setShellOverlay(false)
            return
        }

        activeSystemPanel = panel
        launcherVisible = panel === "launcher"
        aiVisible = panel === "ai"
        controlCenterVisible = panel === "control-center"
        notificationCenterVisible = panel === "notification-center"
        mokoWindowManager.setShellOverlay(true)
        if (notificationCenterVisible) {
            mokoNotificationModel.reportCenterOpened()
            mokoNotificationModel.markAllRead()
        }
        if (aiVisible)
            Qt.callLater(aiPanel.focusInput)
    }

    function dismissSystemPanels() {
        activeSystemPanel = ""
        launcherVisible = false
        aiVisible = false
        controlCenterVisible = false
        notificationCenterVisible = false
        mokoWindowManager.setShellOverlay(false)
    }

    function updateClock() {
        const d = new Date()
        clockText = Qt.formatTime(d, Qt.locale().timeFormat(Locale.ShortFormat))
        dateText = Qt.formatDate(d, Qt.locale().dateFormat(Locale.LongFormat))
                   + "\n" + d.toString()
    }

    Component.onCompleted: updateClock()
    Timer { interval: 1000; repeat: true; running: true; onTriggered: window.updateClock() }

    Image {
        anchors.fill: parent
        source: "qrc:/qt/qml/MokoShell/assets/wallpaper-desktop.png"
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
        dateText: window.dateText
        control: mokoSystemControl
        notificationModel: mokoNotificationModel
        windowManager: mokoWindowManager
        z: 10
        onControlCenterRequested: (page) => {
            controlCenter.currentPage = page
            window.toggleSystemPanel("control-center")
            if (window.controlCenterVisible) {
                mokoSystemControl.reportControlCenterOpened(page)
            }
        }
        onNotificationCenterRequested: window.toggleSystemPanel("notification-center")
        onScreenshotRequested: mokoScreenshotController.captureFullScreen()
        onKeyboardLayoutRequested: mokoWindowManager.toggleKeyboardLayout()
    }

    ControlCenterPanel {
        id: controlCenter
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.top: topBar.bottom
        anchors.topMargin: 10
        visible: window.controlCenterVisible
        control: mokoSystemControl
        windowManager: mokoWindowManager
        z: 15
    }

    NotificationCenterPanel {
        id: notificationCenter
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.top: topBar.bottom
        anchors.topMargin: 10
        visible: window.notificationCenterVisible
        notificationModel: mokoNotificationModel
        z: 16
    }

    LauncherPanel {
        id: launcher
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.top: topBar.bottom
        anchors.topMargin: 24
        visible: launcherVisible
        applicationModel: mokoLauncherApplications
        opacity: visible ? 1 : 0
        z: 4
        onAppRequested: (appId) => window.openApplication(appId)
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
        controller: mokoAiController
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
            font.pixelSize: 118
            font.weight: Font.Black
            font.letterSpacing: 0
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "OS"
            color: "#4C82FF"
            font.pixelSize: 38
            font.letterSpacing: 0
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "v0.1.1  •  HARDWARE & USABILITY PREVIEW"
            color: "#657487"
            font.pixelSize: 10
            font.letterSpacing: 0
        }
    }

    Dock {
        applicationModel: mokoDockApplications
        windowManager: mokoWindowManager
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        z: 7
        onLauncherRequested: window.toggleSystemPanel("launcher")
        onAiRequested: window.toggleSystemPanel("ai")
        onAppRequested: (appId) => window.openApplication(appId)
    }

    Connections {
        target: mokoWindowManager
        function onGlobalActionRequested(action) {
            if (action === "screenshot") {
                mokoScreenshotController.captureFullScreen()
            } else if (action === "notification-center") {
                window.toggleSystemPanel("notification-center")
            } else if (action === "launcher") {
                window.toggleSystemPanel("launcher")
            } else if (action === "ai") {
                window.toggleSystemPanel("ai")
            }
        }
    }

    Connections {
        target: mokoNotificationModel
        function onNotificationReceived(summary, body) {
            toast.show(body.length > 0 ? summary + ": " + body : summary)
        }
    }

    Connections {
        target: mokoScreenshotController
        function onScreenshotSaved(path) {
            mokoNotificationModel.addLocalNotification("Screenshot saved", path)
        }
        function onScreenshotFailed(message) {
            toast.show(message)
        }
    }

    Connections {
        target: mokoApplicationRegistry
        function onApplicationLaunching(appId, displayName) {
            toast.show("Launching " + displayName + "...")
        }
        function onApplicationRunning(appId, displayName) {
            toast.show(displayName + " is running")
        }
        function onApplicationFailed(appId, displayName, message) {
            toast.show("Could not launch " + displayName + ": " + message)
        }
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
        Text {
            id: toastText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 20
            color: "white"
            font.pixelSize: 12
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
        Timer { id: hideTimer; interval: 1800; onTriggered: toast.visible = false }
    }

    Shortcut { sequence: "Meta+Space"; onActivated: window.toggleSystemPanel("launcher") }
    Shortcut { sequence: "Meta+A"; onActivated: window.toggleSystemPanel("ai") }
    Shortcut { sequence: "Meta+N"; onActivated: window.toggleSystemPanel("notification-center") }
    Shortcut { sequence: "Ctrl+Space"; onActivated: mokoWindowManager.toggleKeyboardLayout() }
    Shortcut { sequence: "Print"; onActivated: mokoScreenshotController.captureFullScreen() }
    Shortcut {
        sequence: "Ctrl+Alt+A"
        onActivated: {
            if (window.activeSystemPanel !== "ai")
                window.toggleSystemPanel("ai")
            aiPanel.focusInput()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: window.dismissSystemPanels()
    }
}
