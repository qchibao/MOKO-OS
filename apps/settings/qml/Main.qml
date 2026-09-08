import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 1040
    height: 700
    minimumWidth: 820
    minimumHeight: 560
    title: "MOKO Settings"
    color: "#F7FBFF"
    flags: Qt.Window | Qt.FramelessWindowHint

    property var sections: {
        const developerMode = mokoSettings.developerMode
        return mokoSettings.sectionIds().filter(function(sectionId) {
            return developerMode || sectionId !== "system"
        })
    }
    property string sectionQuery: ""
    property var filteredSections: sections.filter(function(sectionId) {
        if (!window.sectionQuery.trim().length)
            return true
        const needle = window.sectionQuery.trim().toLowerCase()
        return mokoSettings.sectionTitle(sectionId).toLowerCase().indexOf(needle) >= 0
               || mokoSettings.sectionDescription(sectionId).toLowerCase().indexOf(needle) >= 0
    })
    property string selectedSection: sections.length ? sections[0] : "about"
    readonly property string appearanceMode: mokoSettings.appearanceMode
    readonly property color pageColor: appearanceMode === "dark" ? "#0E1723"
                                                        : appearanceMode === "glass" ? "#EAF5FE" : "#F7FBFF"
    readonly property color panelColor: appearanceMode === "dark" ? "#172334" : "#FFFFFF"
    readonly property color cardColor: appearanceMode === "dark" ? "#1D2D40" : "#F7FAFD"
    readonly property color primaryText: appearanceMode === "dark" ? "#F2F7FD" : "#111923"
    readonly property color secondaryText: appearanceMode === "dark" ? "#B7C7D9" : "#687A8B"
    readonly property color lineColor: appearanceMode === "dark" ? "#30445D" : "#DCE7EF"
    property var currentRows: {
        mokoSettings.developerMode
        return mokoSettings.rows(selectedSection)
    }

    function selectSection(sectionId) {
        if (selectedSection !== sectionId)
            selectedSection = sectionId
        else
            refreshSelectedSection()
    }

    function refreshSelectedSection() {
        mokoSettings.refreshSection(selectedSection)
        currentRows = mokoSettings.rows(selectedSection)
        if (selectedSection === "network" && mokoSystemControl)
            mokoSystemControl.preloadNetwork()
        if (selectedSection === "bluetooth" && mokoSystemControl)
            mokoSystemControl.preloadBluetooth()
    }

    onSelectedSectionChanged: refreshSectionTimer.restart()

    Timer {
        id: refreshSectionTimer
        interval: 100
        repeat: false
        onTriggered: refreshSelectedSection()
    }

    Component.onCompleted: refreshSectionTimer.start()

    Connections {
        target: mokoSettings
        function onDataChanged() { window.currentRows = mokoSettings.rows(window.selectedSection) }
        function onDeveloperModeChanged() {
            if (!mokoSettings.developerMode && window.selectedSection === "system")
                window.selectSection("about")
            else
                window.currentRows = mokoSettings.rows(window.selectedSection)
        }
    }

    Rectangle { anchors.fill: parent; color: window.pageColor }

    Rectangle {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 54
        z: 10
        color: window.appearanceMode === "dark" ? "#111D2B" : "#F2F8FD"
        border.color: window.lineColor
        MouseArea {
            anchors.fill: parent
            onPressed: window.startSystemMove()
            onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                ? Window.Windowed : Window.Maximized
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 12
            spacing: 9
            Text { text: "MOKO"; color: window.primaryText; font.pixelSize: 20; font.weight: Font.Black }
            Text { text: "SETTINGS"; color: "#3978F6"; font.pixelSize: 11; font.weight: Font.Bold }
            Item { Layout.fillWidth: true }
            Button {
                text: "-"
                implicitWidth: 36
                implicitHeight: 32
                ToolTip.visible: hovered
                ToolTip.text: "Minimize MOKO Settings"
                background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "transparent"; border.color: window.lineColor }
                contentItem: Text { text: parent.text; color: window.primaryText; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: window.showMinimized()
            }
            Button {
                text: window.visibility === Window.Maximized ? "o" : "[]"
                implicitWidth: 38
                implicitHeight: 32
                ToolTip.visible: hovered
                ToolTip.text: window.visibility === Window.Maximized ? "Restore" : "Maximize"
                background: Rectangle { radius: 6; color: parent.hovered ? "#DCEBFA" : "transparent"; border.color: window.lineColor }
                contentItem: Text { text: parent.text; color: window.primaryText; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: window.visibility = window.visibility === Window.Maximized ? Window.Windowed : Window.Maximized
            }
            Button {
                text: "X"
                implicitWidth: 36
                implicitHeight: 32
                ToolTip.visible: hovered
                ToolTip.text: "Close MOKO Settings"
                background: Rectangle { radius: 6; color: parent.hovered ? "#F0DDE0" : "transparent"; border.color: window.lineColor }
                contentItem: Text { text: parent.text; color: parent.hovered ? "#A53D4B" : window.primaryText; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: window.close()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            color: window.appearanceMode === "dark" ? "#142131" : "#EAF4FC"
            border.color: window.lineColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "PREFERENCES"; color: window.appearanceMode === "dark" ? "#B7C7D9" : "#59718A"; font.pixelSize: 10; font.weight: Font.Bold }
                    Item { Layout.fillWidth: true }
                }

                TextField {
                    id: sectionSearch
                    Layout.fillWidth: true
                    implicitHeight: 40
                    placeholderText: "Search settings..."
                    text: window.sectionQuery
                    selectByMouse: true
                    onTextEdited: window.sectionQuery = text
                    color: window.primaryText
                    placeholderTextColor: "#8798A9"
                    background: Rectangle {
                        radius: 8
                        color: window.appearanceMode === "dark" ? "#1B2C3F" : "#FFFFFF"
                        border.color: sectionSearch.activeFocus ? "#5C91FF" : window.lineColor
                    }
                }

                ListView {
                    id: sectionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: window.filteredSections
                    spacing: 4
                    clip: true
                    currentIndex: 0
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: sectionList.width
                        height: 42
                        radius: 7
                        color: window.selectedSection === modelData ? "#D9E9FF"
                                                                     : sectionMouse.containsMouse ? "#F4FAFF" : "transparent"
                        border.color: window.selectedSection === modelData ? "#AFCBFA" : "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 13
                            anchors.verticalCenter: parent.verticalCenter
                            text: mokoSettings.sectionTitle(modelData)
                            color: window.appearanceMode === "dark" ? "#E2ECF7" : "#182536"
                            font.pixelSize: 13
                            font.weight: window.selectedSection === modelData ? Font.DemiBold : Font.Normal
                        }
                        MouseArea {
                            id: sectionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                sectionList.currentIndex = index
                                window.selectSection(modelData)
                            }
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: mokoSettings.developerMode ? "Developer Mode: On" : "Developer Mode"
                    checkable: true
                    checked: mokoSettings.developerMode
                    implicitHeight: 38
                    background: Rectangle {
                        radius: 7
                        color: parent.checked ? "#DCEAFF"
                                              : parent.hovered ? "#F5FAFF" : window.appearanceMode === "dark" ? "#1B2C3F" : "#FFFFFF"
                        border.color: parent.checked ? "#8EB6F8" : window.lineColor
                    }
                    contentItem: Text {
                        text: parent.text
                        color: window.appearanceMode === "dark" && !parent.checked ? "#DCE8F4" : "#1F3247"
                        font.pixelSize: 11
                        font.weight: parent.checked ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: mokoSettings.developerMode = checked
                }

                Button {
                    Layout.fillWidth: true
                    text: "Refresh"
                    implicitHeight: 38
                    background: Rectangle {
                        radius: 7
                        color: parent.down ? "#D4E5FA" : parent.hovered ? "#F5FAFF" : window.appearanceMode === "dark" ? "#1B2C3F" : "#FFFFFF"
                        border.color: window.lineColor
                    }
                    contentItem: Text {
                        text: parent.text
                        color: window.appearanceMode === "dark" ? "#DCE8F4" : "#1F3247"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: window.refreshSelectedSection()
                }
                Text {
                    Layout.fillWidth: true
                    text: "Updated " + mokoSettings.refreshedAt
                    color: window.secondaryText
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
                    color: window.panelColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text {
                        text: mokoSettings.sectionTitle(window.selectedSection)
                        color: window.primaryText
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: mokoSettings.sectionDescription(window.selectedSection)
                        color: window.secondaryText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "date-time"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 250
                    radius: 8
                    color: window.cardColor
                    border.color: window.lineColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: mokoSettings.localDateTime
                            color: window.primaryText
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Time zone"; color: window.secondaryText; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: timeZoneCombo
                                Layout.preferredWidth: 300
                                model: mokoSettings.timeZoneChoices
                                currentIndex: Math.max(0, mokoSettings.timeZoneChoices.indexOf(mokoSettings.timeZoneId))
                                onActivated: mokoSettings.setTimeZone(currentText)
                            }
                        }
                        Button {
                            Layout.alignment: Qt.AlignRight
                            text: "Use Vietnam time"
                            onClicked: mokoSettings.setTimeZone("Asia/Ho_Chi_Minh")
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "Set time automatically"; color: window.secondaryText; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.automaticTime
                                enabled: mokoSettings.automaticTimeAvailable
                                onToggled: mokoSettings.setAutomaticTime(checked)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "24-hour clock"; color: window.secondaryText; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.twentyFourHour
                                onToggled: mokoSettings.twentyFourHour = checked
                            }
                        }
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "trackpad"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 330
                    radius: 8
                    color: window.cardColor
                    border.color: window.lineColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12
                        Text {
                            text: mokoSettings.trackpadAvailable
                                  ? mokoSettings.trackpadCount + " trackpad detected"
                                  : "No compatible trackpad detected"
                            color: window.primaryText
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "Natural scrolling"; color: window.secondaryText; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.naturalScrollEnabled
                                enabled: mokoSettings.naturalScrollAvailable
                                onToggled: mokoSettings.setNaturalScrollEnabled(checked)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "Three-finger window drag"; color: window.secondaryText; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.threeFingerDragEnabled
                                enabled: mokoSettings.threeFingerDragAvailable
                                onToggled: mokoSettings.setThreeFingerDragEnabled(checked)
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: mokoSettings.threeFingerDragAvailable
                                  ? "Move the active window with three fingers."
                                  : "Three-finger drag is not supported by this device."
                            color: window.secondaryText
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "Browser history swipe"; color: window.secondaryText; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.browserHistorySwipeEnabled
                                enabled: mokoSettings.browserHistorySwipeAvailable
                                onToggled: mokoSettings.setBrowserHistorySwipeEnabled(checked)
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Two-finger horizontal swipes navigate Browser history; vertical scrolling remains unchanged."
                            color: window.secondaryText
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "appearance"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    radius: 8
                    color: window.appearanceMode === "dark" ? "#172334" : "#F7FAFD"
                    border.color: window.appearanceMode === "dark" ? "#30445D" : "#DCE7EF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14
                        Text {
                            text: "MOKO appearance"
                            color: window.appearanceMode === "dark" ? "#F2F7FD" : "#16263B"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: mokoSettings.safeGraphics
                                  ? "Safe Graphics is active; Glass uses reduced effects."
                                  : "Choose the presentation that feels right for this session."
                            color: window.appearanceMode === "dark" ? "#B7C7D9" : "#718294"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: ["light", "dark", "glass"]
                                delegate: Button {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 48
                                    text: modelData === "light" ? "MOKO Light"
                                          : modelData === "dark" ? "MOKO Dark" : "Glass"
                                    checkable: true
                                    checked: mokoSettings.appearanceMode === modelData
                                    onClicked: mokoSettings.setAppearanceMode(modelData)
                                    background: Rectangle {
                                        radius: 7
                                        color: parent.checked ? "#3978F6" : window.appearanceMode === "dark" ? "#22344A" : "#FFFFFF"
                                        border.color: parent.checked ? "#3978F6" : window.appearanceMode === "dark" ? "#405871" : "#C7D9EA"
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.checked ? "#FFFFFF" : window.appearanceMode === "dark" ? "#DCE8F4" : "#26384C"
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Blue remains the default accent. Theme changes apply to the current MOKO session."
                            color: window.appearanceMode === "dark" ? "#9DB0C5" : "#718294"
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "network"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#F7FAFD"
                    border.color: "#DCE7EF"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 9
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: mokoSystemControl && mokoSystemControl.wifiAvailable
                                      ? (mokoSystemControl.activeSsid || mokoSystemControl.wifiState)
                                      : "Wi-Fi not detected"
                                color: "#16263B"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Switch {
                                checked: mokoSystemControl && mokoSystemControl.wifiEnabled
                                enabled: mokoSystemControl && mokoSystemControl.wifiAvailable
                                onToggled: mokoSystemControl.setWifiEnabled(checked)
                            }
                            Button {
                                text: mokoSystemControl && mokoSystemControl.wifiScanning ? "Scanning" : "Rescan"
                                enabled: mokoSystemControl && mokoSystemControl.wifiAvailable
                                         && mokoSystemControl.wifiEnabled && !mokoSystemControl.wifiScanning
                                onClicked: mokoSystemControl.requestWifiScan()
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: mokoSystemControl && mokoSystemControl.networkBusy
                                  ? mokoSystemControl.operationMessage
                                  : "Nearby networks"
                            color: "#718294"
                            font.pixelSize: 10
                        }
                        ListView {
                            id: settingsWifiList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: mokoSystemControl && mokoSystemControl.wifiEnabled
                                   ? mokoSystemControl.wifiNetworks : []
                            delegate: Item {
                                required property var modelData
                                width: settingsWifiList.width
                                height: 48
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 8
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.ssid + "  " + modelData.strength + "%"
                                        color: "#26384C"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: modelData.secure ? "Secured" : "Open"; color: "#718294"; font.pixelSize: 9 }
                                    Button {
                                        text: modelData.connected ? "Disconnect" : "Connect"
                                        enabled: !mokoSystemControl.networkBusy
                                        onClicked: {
                                            if (modelData.connected)
                                                mokoSystemControl.disconnectWifi()
                                            else if (modelData.secure) {
                                                settingsWifiPassword.text = ""
                                                settingsWifiName.text = modelData.ssid
                                                settingsWifiDialog.open()
                                            } else
                                                mokoSystemControl.connectWifi(modelData.ssid, "")
                                        }
                                    }
                                }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#E2EAF1" }
                            }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: settingsManualSsid
                                Layout.fillWidth: true
                                placeholderText: "Other network name"
                            }
                            Button {
                                text: "Join"
                                enabled: settingsManualSsid.text.length > 0 && mokoSystemControl && !mokoSystemControl.networkBusy
                                onClicked: {
                                    settingsWifiName.text = settingsManualSsid.text
                                    settingsWifiPassword.text = ""
                                    settingsWifiDialog.open()
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "bluetooth"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#F7FAFD"
                    border.color: "#DCE7EF"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 9
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: mokoSystemControl && mokoSystemControl.bluetoothAvailable
                                      ? (mokoSystemControl.bluetoothScanning ? "Scanning for devices" : "Bluetooth ready")
                                      : "Bluetooth not detected"
                                color: "#16263B"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Switch {
                                checked: mokoSystemControl && mokoSystemControl.bluetoothPowered
                                enabled: mokoSystemControl && mokoSystemControl.bluetoothAvailable
                                onToggled: mokoSystemControl.setBluetoothPowered(checked)
                            }
                            Button {
                                text: mokoSystemControl && mokoSystemControl.bluetoothScanning ? "Stop" : "Scan"
                                enabled: mokoSystemControl && mokoSystemControl.bluetoothPowered
                                onClicked: mokoSystemControl.setBluetoothScanning(!mokoSystemControl.bluetoothScanning)
                            }
                        }
                        ListView {
                            id: settingsBluetoothList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: mokoSystemControl ? mokoSystemControl.bluetoothDevices : []
                            delegate: Item {
                                required property var modelData
                                width: settingsBluetoothList.width
                                height: 52
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 8
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Text { text: modelData.name; color: "#26384C"; font.pixelSize: 11; elide: Text.ElideRight }
                                        Text { text: modelData.connected ? "Connected" : modelData.paired ? "Paired" : "Available"; color: "#718294"; font.pixelSize: 9 }
                                    }
                                    Button {
                                        text: modelData.connected ? "Disconnect" : modelData.paired ? "Connect" : "Pair"
                                        enabled: !mokoSystemControl.bluetoothBusy
                                        onClicked: modelData.connected
                                            ? mokoSystemControl.disconnectBluetoothDevice(modelData.id)
                                            : modelData.paired
                                              ? mokoSystemControl.connectBluetoothDevice(modelData.id)
                                              : mokoSystemControl.pairBluetoothDevice(modelData.id)
                                    }
                                    Button {
                                        text: "Forget"
                                        visible: modelData.paired
                                        onClicked: mokoSystemControl.forgetBluetoothDevice(modelData.id)
                                    }
                                }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#E2EAF1" }
                            }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                    }
                }

                ListView {
                    id: settingRows
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.selectedSection !== "date-time"
                             && window.selectedSection !== "trackpad"
                             && window.selectedSection !== "appearance"
                             && window.selectedSection !== "network"
                             && window.selectedSection !== "bluetooth"
                    model: window.currentRows
                    spacing: 8
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property var modelData
                        width: settingRows.width
                        height: mokoSettings.developerMode ? 92 : 66
                        radius: 8
                        color: "#F7FAFD"
                        border.color: modelData.available ? "#DCE7EF" : "#E5E9ED"

                        RowLayout {
                            id: rowContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 18

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: modelData.available ? "#223145" : "#7E8994"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                }
                                Text {
                                    visible: mokoSettings.developerMode
                                             && modelData.detail && modelData.detail.length > 0
                                    Layout.fillWidth: true
                                    text: modelData.detail || ""
                                    color: "#758698"
                                    font.pixelSize: 9
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 5
                                    elide: Text.ElideRight
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 300
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.value
                                    color: modelData.available ? "#15243A" : "#8E99A4"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignRight
                                    wrapMode: Text.WrapAnywhere
                                }
                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: modelData.writable ? "Writable" : "Read-only"
                                    color: modelData.writable ? "#2E7D62" : "#8996A3"
                                    font.pixelSize: 8
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }

                Button {
                    visible: window.selectedSection === "hardware"
                    Layout.fillWidth: true
                    implicitHeight: 42
                    text: "Open Hardware Diagnostics"
                    onClicked: mokoSettings.openHardwareDiagnostics()
                    background: Rectangle {
                        radius: 7
                        color: parent.down ? "#2F68D7" : parent.hovered ? "#4D85F5" : "#3978F6"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 7
                    color: "#ECF5FF"
                    border.color: "#D3E5F6"
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: mokoSettings.statusMessage
                              + ". Changes appear only when this device supports them."
                        color: "#567087"
                        font.pixelSize: 10
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Dialog {
        id: settingsWifiDialog
        anchors.centerIn: parent
        modal: true
        title: "Join Wi-Fi network"
        width: Math.min(420, window.width - 48)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: mokoSystemControl.connectWifi(settingsWifiName.text,
                                                   settingsWifiPassword.text)
        ColumnLayout {
            width: 370
            spacing: 10
            TextField {
                id: settingsWifiName
                Layout.fillWidth: true
                placeholderText: "Network name"
            }
            TextField {
                id: settingsWifiPassword
                Layout.fillWidth: true
                placeholderText: "Password (leave empty for an open network)"
                echoMode: TextInput.Password
                onAccepted: settingsWifiDialog.accept()
            }
            Text {
                Layout.fillWidth: true
                text: "The password is sent only to NetworkManager for this connection."
                color: "#718294"
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }
        }
    }

    Dialog {
        id: settingsBluetoothPrompt
        anchors.centerIn: parent
        modal: true
        visible: mokoSystemControl && mokoSystemControl.bluetoothPromptVisible
        title: mokoSystemControl ? mokoSystemControl.bluetoothPromptTitle : "Bluetooth confirmation"
        width: Math.min(430, window.width - 48)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: mokoSystemControl.answerBluetoothPrompt(
                        bluetoothPromptInput.visible ? bluetoothPromptInput.text : "", true)
        onRejected: mokoSystemControl.answerBluetoothPrompt("", false)
        ColumnLayout {
            width: 380
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: mokoSystemControl ? mokoSystemControl.bluetoothPromptMessage : ""
                color: "#26384C"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
            TextField {
                id: bluetoothPromptInput
                Layout.fillWidth: true
                visible: mokoSystemControl && mokoSystemControl.bluetoothPromptNeedsInput
                text: mokoSystemControl ? mokoSystemControl.bluetoothPromptValue : ""
                inputMethodHints: Qt.ImhDigitsOnly
            }
        }
    }

    MouseArea {
        width: 14
        height: 14
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        cursorShape: Qt.SizeFDiagCursor
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }

    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
                                       ? Window.Windowed : Window.FullScreen
    }
}
