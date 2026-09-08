import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    property var control
    property var windowManager
    property int currentPage: 0
    property string pendingWifiSsid: ""
    property string pendingBluetoothId: ""
    property string pendingBluetoothName: ""
    property int pendingScale: 100
    property int previousScale: 100
    property int scaleCountdown: 15

    width: Math.min(410, parent ? parent.width - 32 : 410)
    height: Math.min(720, parent ? parent.height - 78 : 720)
    radius: 14
    glassOpacity: .97
    clip: true

    function openWifiPassword(ssid) {
        pendingWifiSsid = ssid
        wifiPassword.clear()
        wifiDialog.visible = true
        Qt.callLater(function() { wifiPassword.forceActiveFocus() })
    }

    function confirmForget(deviceId, deviceName) {
        pendingBluetoothId = deviceId
        pendingBluetoothName = deviceName
        forgetDialog.visible = true
    }

    function tryScale(value) {
        if (!windowManager || value === windowManager.outputScale)
            return
        previousScale = windowManager.outputScale
        if (!windowManager.setOutputScale(value))
            return
        pendingScale = value
        scaleCountdown = 15
        scaleDialog.visible = true
        scaleTimer.restart()
    }

    function keepScale() {
        if (windowManager)
            windowManager.saveOutputScale(pendingScale)
        scaleTimer.stop()
        scaleDialog.visible = false
    }

    function revertScale() {
        if (windowManager)
            windowManager.setOutputScale(previousScale)
        scaleTimer.stop()
        scaleDialog.visible = false
    }

    Timer {
        id: scaleTimer
        interval: 1000
        repeat: true
        onTriggered: {
            scaleCountdown--
            if (scaleCountdown <= 0)
                root.revertScale()
        }
    }

    function signalBars(strength) {
        if (strength >= 75)
            return "Strong"
        if (strength >= 45)
            return "Good"
        return "Weak"
    }

    onCurrentPageChanged: {
        if (visible && control)
            control.reportControlCenterOpened(currentPage)
        if (visible && currentPage === 4 && windowManager)
            windowManager.reportInputPanelOpened()
    }

    component SectionTitle: RowLayout {
        property string title
        property string detail
        Layout.fillWidth: true
        spacing: 8
        Text {
            text: parent.title
            color: "#1E2936"
            font.pixelSize: 14
            font.bold: true
        }
        Item { Layout.fillWidth: true }
        Text {
            text: parent.detail
            color: "#718092"
            font.pixelSize: 10
            elide: Text.ElideRight
            Layout.maximumWidth: 190
        }
    }

    component Separator: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Qt.rgba(.34,.48,.63,.13)
    }

    component EmptyState: Text {
        property string message
        Layout.fillWidth: true
        Layout.preferredHeight: 38
        text: message
        color: "#7B8998"
        font.pixelSize: 11
        wrapMode: Text.Wrap
        verticalAlignment: Text.AlignVCenter
    }

    component DeviceCombo: ComboBox {
        id: combo
        property var controlModel
        model: controlModel
        textRole: "name"
        valueRole: "id"
        implicitHeight: 35
        leftPadding: 11
        rightPadding: 30

        contentItem: Text {
            leftPadding: 2
            text: combo.displayText
            color: combo.enabled ? "#334252" : "#96A4B2"
            font.pixelSize: 10
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Canvas {
            x: combo.width - width - 11
            y: (combo.height - height) / 2
            width: 10
            height: 7
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#667688"
                ctx.lineWidth = 1.5
                ctx.beginPath()
                ctx.moveTo(1, 1)
                ctx.lineTo(width / 2, height - 1)
                ctx.lineTo(width - 1, 1)
                ctx.stroke()
            }
        }
        background: Rectangle {
            radius: 7
            color: Qt.rgba(1,1,1,.58)
            border.color: Qt.rgba(.35,.50,.65,.17)
        }
        popup: Popup {
            y: combo.height + 3
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)
            padding: 4
            background: Rectangle {
                radius: 7
                color: "#F7FBFF"
                border.color: "#CBD9E6"
            }
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
            }
        }
        delegate: ItemDelegate {
            required property var model
            width: combo.width - 8
            height: 34
            contentItem: Text {
                text: model.name
                color: "#334252"
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                radius: 5
                color: parent.highlighted ? "#E5F0FC" : "transparent"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Control Center"
                color: "#141C26"
                font.pixelSize: 18
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            MokoActionButton {
                text: "Refresh"
                implicitWidth: 74
                onClicked: if (root.control) root.control.refresh()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: ["Connections", "Sound", "Display", "Power", "Input"]
                delegate: Button {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 34
                    contentItem: Text {
                        text: modelData
                        color: index === root.currentPage ? "#205DB9" : "#5C6B7B"
                        font.pixelSize: 10
                        font.bold: index === root.currentPage
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 7
                        color: index === root.currentPage ? "#E1EEFC" : "transparent"
                    }
                    onClicked: root.currentPage = index
                }
            }
        }

        Separator {}

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            ScrollView {
                id: connectionsPage
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: connectionsPage.availableWidth
                    spacing: 10

                    SectionTitle {
                        title: "Wi-Fi"
                        detail: !root.control || !root.control.networkManagerAvailable ? "Unavailable"
                                : root.control.activeSsid ? root.control.activeSsid
                                : root.control.wifiState
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.control && root.control.wifiAvailable
                                  ? (root.control.wifiEnabled ? "Wireless networking is on" : "Wireless networking is off")
                                  : "No Wi-Fi device detected"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        MokoActionButton {
                            text: root.control && root.control.wifiScanning ? "Scanning" : "Scan"
                            enabled: root.control && root.control.wifiAvailable
                                     && root.control.wifiEnabled && !root.control.wifiScanning
                            onClicked: root.control.requestWifiScan()
                        }
                        MokoSwitch {
                            checked: root.control ? root.control.wifiEnabled : false
                            enabled: root.control && root.control.wifiAvailable
                            onToggled: root.control.setWifiEnabled(checked)
                        }
                    }

                    EmptyState {
                        visible: !root.control || !root.control.wifiEnabled
                                 || root.control.wifiNetworks.length === 0
                        message: !root.control || !root.control.wifiAvailable
                                 ? "Wi-Fi hardware is not available."
                                 : !root.control.wifiEnabled
                                   ? "Turn on Wi-Fi to see nearby networks."
                                   : "No nearby networks found."
                    }

                    Repeater {
                        model: root.control && root.control.wifiEnabled
                               ? root.control.wifiNetworks : []
                        delegate: Item {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48

                            RowLayout {
                                anchors.fill: parent
                                spacing: 9
                                StatusIcon {
                                    width: 20
                                    height: 20
                                    kind: "wifi"
                                    active: true
                                    level: modelData.strength
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.ssid
                                        color: "#273442"
                                        font.pixelSize: 11
                                        font.bold: modelData.connected
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: (modelData.secure ? "Secured" : "Open") + " - "
                                              + root.signalBars(modelData.strength)
                                        color: "#7A8999"
                                        font.pixelSize: 9
                                    }
                                }
                                MokoActionButton {
                                    text: modelData.connected ? "Disconnect" : "Connect"
                                    accent: !modelData.connected
                                    enabled: !root.control.networkBusy
                                    onClicked: {
                                        if (modelData.connected)
                                            root.control.disconnectWifi()
                                        else if (modelData.secure)
                                            root.openWifiPassword(modelData.ssid)
                                        else
                                            root.control.connectWifi(modelData.ssid, "")
                                    }
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: Qt.rgba(.34,.48,.63,.10)
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 4 }
                    Separator {}
                    Item { Layout.preferredHeight: 2 }

                    SectionTitle {
                        title: "Bluetooth"
                        detail: !root.control || !root.control.bluetoothAvailable ? "Not detected"
                                : root.control.bluetoothPowered ? "On" : "Off"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.control && root.control.bluetoothScanning
                                  ? "Looking for nearby devices"
                                  : "Pair and connect devices"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        MokoActionButton {
                            text: root.control && root.control.bluetoothScanning ? "Stop" : "Scan"
                            enabled: root.control && root.control.bluetoothAvailable
                                     && root.control.bluetoothPowered && !root.control.bluetoothBusy
                            onClicked: root.control.setBluetoothScanning(!root.control.bluetoothScanning)
                        }
                        MokoSwitch {
                            checked: root.control ? root.control.bluetoothPowered : false
                            enabled: root.control && root.control.bluetoothAvailable
                            onToggled: root.control.setBluetoothPowered(checked)
                        }
                    }

                    EmptyState {
                        visible: !root.control || !root.control.bluetoothPowered
                                 || root.control.bluetoothDevices.length === 0
                        message: !root.control || !root.control.bluetoothAvailable
                                 ? "No Bluetooth controller detected."
                                 : !root.control.bluetoothPowered
                                   ? "Turn on Bluetooth to see devices."
                                   : "No Bluetooth devices found yet."
                    }

                    Repeater {
                        model: root.control && root.control.bluetoothPowered
                               ? root.control.bluetoothDevices : []
                        delegate: Item {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: modelData.paired ? 58 : 48

                            RowLayout {
                                anchors.fill: parent
                                spacing: 9
                                StatusIcon {
                                    width: 19
                                    height: 19
                                    kind: "bluetooth"
                                    active: modelData.connected
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: "#273442"
                                        font.pixelSize: 11
                                        font.bold: modelData.connected
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.connected ? "Connected"
                                              : modelData.paired ? "Paired" : "Available"
                                        color: "#7A8999"
                                        font.pixelSize: 9
                                    }
                                }
                                MokoActionButton {
                                    text: !modelData.paired ? "Pair"
                                          : modelData.connected ? "Disconnect" : "Connect"
                                    accent: !modelData.connected
                                    enabled: !root.control.bluetoothBusy
                                    onClicked: {
                                        if (!modelData.paired)
                                            root.control.pairBluetoothDevice(modelData.id)
                                        else if (modelData.connected)
                                            root.control.disconnectBluetoothDevice(modelData.id)
                                        else
                                            root.control.connectBluetoothDevice(modelData.id)
                                    }
                                }
                                MokoActionButton {
                                    visible: modelData.paired
                                    text: "Forget"
                                    danger: true
                                    implicitWidth: 57
                                    enabled: !root.control.bluetoothBusy
                                    onClicked: root.confirmForget(modelData.id, modelData.name)
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: Qt.rgba(.34,.48,.63,.10)
                            }
                        }
                    }
                }
            }

            ScrollView {
                id: soundPage
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: soundPage.availableWidth
                    spacing: 12

                    SectionTitle {
                        title: "Sound"
                        detail: root.control && root.control.audioAvailable ? "Ready" : "Unavailable"
                    }
                    EmptyState {
                        visible: !root.control || !root.control.audioAvailable
                        message: "PipeWire audio controls are not available in this session."
                    }

                    Text {
                        visible: root.control && root.control.audioAvailable
                        text: "Output"
                        color: "#536274"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        visible: root.control && root.control.audioAvailable
                        Layout.fillWidth: true
                        spacing: 10
                        StatusIcon {
                            width: 22
                            height: 22
                            kind: "sound"
                            active: root.control && !root.control.outputMuted
                            level: root.control ? root.control.outputVolume : 0
                        }
                        MokoSlider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: root.control ? Math.min(100, root.control.outputVolume) : 0
                            enabled: root.control && root.control.audioAvailable
                            onMoved: root.control.setOutputVolume(Math.round(value))
                        }
                        Text {
                            text: root.control ? Math.round(root.control.outputVolume) + "%" : "0%"
                            color: "#536274"
                            font.pixelSize: 10
                            Layout.preferredWidth: 34
                        }
                        MokoSwitch {
                            checked: root.control ? !root.control.outputMuted : false
                            enabled: root.control && root.control.audioAvailable
                            onToggled: root.control.setOutputMuted(!checked)
                        }
                    }
                    DeviceCombo {
                        visible: root.control && root.control.audioAvailable
                        Layout.fillWidth: true
                        controlModel: root.control ? root.control.outputDevices : []
                        enabled: count > 0
                        currentIndex: {
                            for (let index = 0; index < count; ++index) {
                                if (model[index].active)
                                    return index
                            }
                            return count > 0 ? 0 : -1
                        }
                        onActivated: root.control.setOutputDevice(currentValue)
                    }

                    Separator { visible: root.control && root.control.audioAvailable }

                    Text {
                        visible: root.control && root.control.audioAvailable
                        text: "Microphone"
                        color: "#536274"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        visible: root.control && root.control.audioAvailable
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            width: 22
                            height: 22
                            radius: 11
                            color: root.control && root.control.inputMuted ? "#CBD5DF" : "#3475E7"
                            Text {
                                anchors.centerIn: parent
                                text: "M"
                                color: "white"
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                        MokoSlider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: root.control ? Math.min(100, root.control.inputVolume) : 0
                            enabled: root.control && root.control.audioAvailable
                            onMoved: root.control.setInputVolume(Math.round(value))
                        }
                        Text {
                            text: root.control ? Math.round(root.control.inputVolume) + "%" : "0%"
                            color: "#536274"
                            font.pixelSize: 10
                            Layout.preferredWidth: 34
                        }
                        MokoSwitch {
                            checked: root.control ? !root.control.inputMuted : false
                            enabled: root.control && root.control.audioAvailable
                            onToggled: root.control.setInputMuted(!checked)
                        }
                    }
                    DeviceCombo {
                        visible: root.control && root.control.audioAvailable
                        Layout.fillWidth: true
                        controlModel: root.control ? root.control.inputDevices : []
                        enabled: count > 0
                        currentIndex: {
                            for (let index = 0; index < count; ++index) {
                                if (model[index].active)
                                    return index
                            }
                            return count > 0 ? 0 : -1
                        }
                        onActivated: root.control.setInputDevice(currentValue)
                    }
                }
            }

            ScrollView {
                id: displayPage
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: displayPage.availableWidth
                    spacing: 12

                    SectionTitle {
                        title: "Display"
                        detail: root.control && root.control.brightnessAvailable ? "Adjustable" : "Read only"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        StatusIcon {
                            width: 22
                            height: 22
                            kind: "brightness"
                            active: root.control && root.control.brightnessAvailable
                        }
                        MokoSlider {
                            Layout.fillWidth: true
                            from: 1
                            to: 100
                            value: root.control ? root.control.brightness : 50
                            enabled: root.control && root.control.brightnessAvailable
                            onMoved: root.control.setBrightness(Math.round(value))
                        }
                        Text {
                            text: root.control && root.control.brightnessAvailable
                                  ? Math.round(root.control.brightness) + "%" : "N/A"
                            color: "#536274"
                            font.pixelSize: 10
                            Layout.preferredWidth: 36
                        }
                    }
                    EmptyState {
                        visible: !root.control || !root.control.brightnessAvailable
                        message: "This display does not expose writable backlight control."
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Hardware brightness keys use the same backlight control when the compositor receives them."
                        color: "#7B8998"
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                    }

                    Separator {}

                    SectionTitle {
                        title: "Interface scale"
                        detail: root.windowManager && root.windowManager.desktopProtocolAvailable
                                ? root.windowManager.outputScale + "%" : "Unavailable"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        MokoActionButton {
                            Layout.fillWidth: true
                            text: "100%"
                            accent: root.windowManager && root.windowManager.outputScale === 100
                            enabled: root.windowManager && root.windowManager.desktopProtocolAvailable
                            onClicked: root.tryScale(100)
                        }
                        MokoActionButton {
                            Layout.fillWidth: true
                            text: "200%"
                            accent: root.windowManager && root.windowManager.outputScale === 200
                            enabled: root.windowManager && root.windowManager.desktopProtocolAvailable
                                     && root.windowManager.outputScale200Available
                            onClicked: root.tryScale(200)
                        }
                    }
                    EmptyState {
                        visible: !root.windowManager || !root.windowManager.desktopProtocolAvailable
                        message: "Scaling requires the MOKO desktop compositor."
                    }
                    EmptyState {
                        visible: root.windowManager && root.windowManager.desktopProtocolAvailable
                                 && !root.windowManager.outputScale200Available
                        message: "200% scaling requires at least 2560 x 1440 pixels."
                    }
                }
            }

            ScrollView {
                id: powerPage
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: powerPage.availableWidth
                    spacing: 12

                    SectionTitle {
                        title: "Battery"
                        detail: root.control && root.control.batteryAvailable
                                ? root.control.batteryPercent + "%" : "Not detected"
                    }
                    RowLayout {
                        visible: root.control && root.control.batteryAvailable
                        Layout.fillWidth: true
                        spacing: 12
                        StatusIcon {
                            width: 32
                            height: 24
                            kind: "battery"
                            level: root.control ? root.control.batteryPercent : 0
                            charging: root.control && root.control.batteryCharging
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: root.control && root.control.batteryCharging ? "Charging"
                                      : root.control ? root.control.batteryState : "Unknown"
                                color: "#263442"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                text: root.control && root.control.batteryTime
                                      ? root.control.batteryTime + " remaining" : ""
                                color: "#748394"
                                font.pixelSize: 10
                            }
                        }
                    }
                    RowLayout {
                        visible: root.control && root.control.batteryAvailable
                                 && root.control.batteryTechnology.length > 0
                        Layout.fillWidth: true
                        Text { text: "Technology"; color: "#556577"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.control ? root.control.batteryTechnology : ""
                            color: "#334252"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.control && root.control.batteryAvailable
                                 && root.control.batteryCycleCount.length > 0
                        Layout.fillWidth: true
                        Text { text: "Cycle count"; color: "#556577"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.control ? root.control.batteryCycleCount : ""
                            color: "#334252"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.control && root.control.batteryAvailable
                                 && root.control.batteryEnergy.length > 0
                        Layout.fillWidth: true
                        Text { text: "Current / full"; color: "#556577"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.control ? root.control.batteryEnergy : ""
                            color: "#334252"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    EmptyState {
                        visible: !root.control || !root.control.batteryAvailable
                        message: "No battery is reported by the kernel."
                    }
                    RowLayout {
                        visible: root.control && root.control.batteryAvailable
                                 && root.control.batteryHealth >= 0
                        Layout.fillWidth: true
                        Text {
                            text: "Battery health"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.control ? root.control.batteryHealth + "%" : ""
                            color: "#334252"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Separator {}

                    SectionTitle {
                        title: "Power mode"
                        detail: root.control && root.control.powerModeAvailable
                                ? root.control.powerMode : "Unavailable"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Repeater {
                            model: root.control ? root.control.powerModes : []
                            delegate: MokoActionButton {
                                required property string modelData
                                Layout.fillWidth: true
                                text: modelData === "power-saver" ? "Saver"
                                      : modelData === "balanced" ? "Balanced"
                                      : modelData === "performance" ? "Performance"
                                      : modelData
                                accent: root.control && root.control.powerMode === modelData
                                enabled: root.control && root.control.powerModeAvailable
                                onClicked: root.control.setPowerMode(modelData)
                            }
                        }
                    }
                    EmptyState {
                        visible: !root.control || !root.control.powerModeAvailable
                        message: "Power profiles are not available on this hardware."
                    }

                    Separator {}

                    SectionTitle {
                        title: "Sleep"
                        detail: root.control && root.control.suspendPending ? "Preparing"
                                : root.control && root.control.suspendAvailable ? "Ready"
                                : "Unavailable"
                    }
                    MokoActionButton {
                        Layout.fillWidth: true
                        text: root.control && root.control.suspendPending ? "Preparing..." : "Suspend"
                        accent: true
                        enabled: root.control && root.control.suspendAvailable
                                 && !root.control.suspendPending
                        onClicked: suspendDialog.visible = true
                    }
                }
            }

            ScrollView {
                id: inputPage
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: inputPage.availableWidth
                    spacing: 11

                    SectionTitle {
                        title: "Trackpad"
                        detail: !root.windowManager || !root.windowManager.inputProtocolAvailable
                                ? "Unavailable"
                                : root.windowManager.touchpadAvailable
                                  ? root.windowManager.touchpadCount + " detected"
                                  : "Not detected"
                    }
                    EmptyState {
                        visible: !root.windowManager || !root.windowManager.touchpadAvailable
                        message: root.windowManager && root.windowManager.inputProtocolAvailable
                                 ? "No compatible trackpad is connected."
                                 : "Input settings require the MOKO desktop compositor."
                    }

                    SectionTitle {
                        title: "Keyboard"
                        detail: root.windowManager && root.windowManager.desktopProtocolAvailable
                                ? root.windowManager.keyboardLayoutName : "Unavailable"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        MokoActionButton {
                            Layout.fillWidth: true
                            text: "English"
                            accent: root.windowManager && root.windowManager.keyboardLayout === 0
                            enabled: root.windowManager && root.windowManager.desktopProtocolAvailable
                            onClicked: root.windowManager.setKeyboardLayout(0)
                        }
                        MokoActionButton {
                            Layout.fillWidth: true
                            text: "Vietnamese"
                            accent: root.windowManager && root.windowManager.keyboardLayout === 1
                            enabled: root.windowManager && root.windowManager.desktopProtocolAvailable
                            onClicked: root.windowManager.setKeyboardLayout(1)
                        }
                    }
                    Separator {}

                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Tap to click"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.tapToClickAvailable
                                  ? (root.windowManager.tapToClickEnabled ? "On" : "Limited")
                                  : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Two-finger scrolling"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.twoFingerScrollAvailable
                                  ? (root.windowManager.twoFingerScrollEnabled ? "On" : "Limited")
                                  : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Secondary click"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.secondaryClickAvailable
                                  ? (root.windowManager.secondaryClickEnabled ? "Two fingers" : "Limited")
                                  : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Tap and drag"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.dragAvailable
                                  ? (root.windowManager.dragEnabled ? "On" : "Limited")
                                  : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Separator { visible: root.windowManager && root.windowManager.touchpadAvailable }

                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Natural scrolling"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        MokoSwitch {
                            checked: root.windowManager
                                     ? root.windowManager.naturalScrollEnabled : false
                            enabled: root.windowManager
                                     && root.windowManager.naturalScrollAvailable
                            onToggled: root.windowManager.setNaturalScrollEnabled(checked)
                        }
                    }

                    Text {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        text: "Pointer speed"
                        color: "#536274"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Slow"
                            color: "#7B8998"
                            font.pixelSize: 9
                        }
                        MokoSlider {
                            Layout.fillWidth: true
                            from: -100
                            to: 100
                            stepSize: 5
                            value: root.windowManager
                                   ? root.windowManager.pointerAcceleration : 0
                            enabled: root.windowManager
                                     && root.windowManager.pointerAccelerationAvailable
                            onMoved: root.windowManager.setPointerAcceleration(Math.round(value))
                        }
                        Text {
                            text: "Fast"
                            color: "#7B8998"
                            font.pixelSize: 9
                        }
                    }

                    Separator { visible: root.windowManager && root.windowManager.touchpadAvailable }

                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Palm rejection"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.palmRejectionManaged
                                  ? "Automatic" : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "While typing"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.disableWhileTypingAvailable
                                  ? (root.windowManager.disableWhileTypingEnabled ? "Protected" : "Limited")
                                  : "Automatic"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    RowLayout {
                        visible: root.windowManager && root.windowManager.touchpadAvailable
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Multi-finger gestures"
                            color: "#556577"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.windowManager && root.windowManager.gesturesAvailable
                                  ? "Available" : "Not available"
                            color: "#334252"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.control && root.control.operationMessage.length > 0
            text: root.control ? root.control.operationMessage : ""
            color: "#647487"
            font.pixelSize: 9
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: scaleDialog
        anchors.fill: parent
        visible: false
        z: 24
        color: Qt.rgba(.11,.16,.22,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(350, parent.width - 36)
            height: 190
            radius: 10
            color: "#F8FBFE"
            border.color: "#C8D7E6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 9
                Text {
                    Layout.fillWidth: true
                    text: "Keep these display settings?"
                    color: "#1E2936"
                    font.pixelSize: 14
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: "Interface scale is now " + root.pendingScale + "%. Reverting in "
                          + root.scaleCountdown + " seconds unless you keep it."
                    color: "#68788A"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
                ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: 15
                    value: root.scaleCountdown
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MokoActionButton { text: "Revert"; onClicked: root.revertScale() }
                    MokoActionButton { text: "Keep"; accent: true; onClicked: root.keepScale() }
                }
            }
        }
    }

    Rectangle {
        id: suspendDialog
        anchors.fill: parent
        visible: false
        z: 23
        color: Qt.rgba(.11,.16,.22,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(330, parent.width - 36)
            height: 155
            radius: 10
            color: "#F8FBFE"
            border.color: "#C8D7E6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 9
                Text {
                    Layout.fillWidth: true
                    text: "Suspend MOKO OS?"
                    color: "#1E2936"
                    font.pixelSize: 14
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: "Open applications will remain available after resume."
                    color: "#68788A"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MokoActionButton {
                        text: "Cancel"
                        onClicked: suspendDialog.visible = false
                    }
                    MokoActionButton {
                        text: "Suspend"
                        accent: true
                        enabled: root.control && !root.control.suspendPending
                        onClicked: {
                            suspendDialog.visible = false
                            root.control.suspend()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: wifiDialog
        anchors.fill: parent
        visible: false
        z: 20
        color: Qt.rgba(.11,.16,.22,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(330, parent.width - 36)
            height: 190
            radius: 10
            color: "#F8FBFE"
            border.color: "#C8D7E6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 9
                Text {
                    text: "Connect to " + root.pendingWifiSsid
                    color: "#1E2936"
                    font.pixelSize: 14
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: "Enter the Wi-Fi password."
                    color: "#68788A"
                    font.pixelSize: 10
                }
                TextField {
                    id: wifiPassword
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    echoMode: TextInput.Password
                    passwordMaskDelay: 700
                    placeholderText: "Password"
                    color: "#273442"
                    font.pixelSize: 11
                    background: Rectangle {
                        radius: 7
                        color: "white"
                        border.color: wifiPassword.activeFocus ? "#5A8FE9" : "#C7D4E1"
                    }
                    onAccepted: connectButton.clicked()
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MokoActionButton {
                        text: "Cancel"
                        onClicked: wifiDialog.visible = false
                    }
                    MokoActionButton {
                        id: connectButton
                        text: "Connect"
                        accent: true
                        enabled: wifiPassword.text.length > 0
                        onClicked: {
                            root.control.connectWifi(root.pendingWifiSsid, wifiPassword.text)
                            wifiDialog.visible = false
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: forgetDialog
        anchors.fill: parent
        visible: false
        z: 21
        color: Qt.rgba(.11,.16,.22,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(330, parent.width - 36)
            height: 155
            radius: 10
            color: "#F8FBFE"
            border.color: "#C8D7E6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 9
                Text {
                    Layout.fillWidth: true
                    text: "Forget " + root.pendingBluetoothName + "?"
                    color: "#1E2936"
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: "You will need to pair this device again."
                    color: "#68788A"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MokoActionButton { text: "Cancel"; onClicked: forgetDialog.visible = false }
                    MokoActionButton {
                        text: "Forget"
                        danger: true
                        onClicked: {
                            root.control.forgetBluetoothDevice(root.pendingBluetoothId)
                            forgetDialog.visible = false
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: pairingDialog
        anchors.fill: parent
        visible: root.control ? root.control.bluetoothPromptVisible : false
        z: 22
        color: Qt.rgba(.11,.16,.22,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(330, parent.width - 36)
            height: root.control && root.control.bluetoothPromptNeedsInput ? 215 : 185
            radius: 10
            color: "#F8FBFE"
            border.color: "#C8D7E6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 9
                Text {
                    Layout.fillWidth: true
                    text: root.control ? root.control.bluetoothPromptTitle : "Bluetooth"
                    color: "#1E2936"
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: root.control ? root.control.bluetoothPromptMessage : ""
                    color: "#68788A"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: root.control && root.control.bluetoothPromptValue.length > 0
                             && !root.control.bluetoothPromptNeedsInput
                    text: root.control ? root.control.bluetoothPromptValue : ""
                    color: "#205DB9"
                    font.pixelSize: 25
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                TextField {
                    id: bluetoothCode
                    visible: root.control && root.control.bluetoothPromptNeedsInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    placeholderText: root.control && root.control.bluetoothPromptKind === "pin"
                                     ? "PIN" : "Passkey"
                    inputMethodHints: Qt.ImhDigitsOnly
                    color: "#273442"
                    font.pixelSize: 11
                    background: Rectangle {
                        radius: 7
                        color: "white"
                        border.color: bluetoothCode.activeFocus ? "#5A8FE9" : "#C7D4E1"
                    }
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MokoActionButton {
                        text: "Cancel"
                        onClicked: root.control.answerBluetoothPrompt("", false)
                    }
                    MokoActionButton {
                        text: "Confirm"
                        accent: true
                        enabled: !root.control || !root.control.bluetoothPromptNeedsInput
                                 || bluetoothCode.text.length > 0
                        onClicked: {
                            root.control.answerBluetoothPrompt(bluetoothCode.text, true)
                            bluetoothCode.clear()
                        }
                    }
                }
            }
        }
    }
}
