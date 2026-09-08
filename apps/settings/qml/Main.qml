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
    property string selectedSection: sections.length ? sections[0] : "about"
    property var currentRows: {
        mokoSettings.developerMode
        return mokoSettings.rows(selectedSection)
    }

    function selectSection(sectionId) {
        selectedSection = sectionId
        currentRows = mokoSettings.rows(sectionId)
    }

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

    Rectangle {
        anchors.fill: parent
        color: "#F7FBFF"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            color: "#EAF4FC"
            border.color: "#D2E1EC"

            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 54
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "MOKO"; color: "#10151C"; font.pixelSize: 22; font.weight: Font.Black }
                    Text { text: "SETTINGS"; color: "#3978F6"; font.pixelSize: 11; font.weight: Font.Bold }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "-"
                        implicitWidth: 34
                        implicitHeight: 32
                        ToolTip.visible: hovered
                        ToolTip.text: "Minimize MOKO Settings"
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#DCEBFA" : "#FFFFFF"
                            border.color: "#C6D7E5"
                        }
                        onClicked: window.showMinimized()
                    }
                    Button {
                        text: window.visibility === Window.Maximized ? "[]" : "[ ]"
                        implicitWidth: 38
                        implicitHeight: 32
                        ToolTip.visible: hovered
                        ToolTip.text: window.visibility === Window.Maximized ? "Restore MOKO Settings"
                                                                            : "Maximize MOKO Settings"
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#DCEBFA" : "#FFFFFF"
                            border.color: "#C6D7E5"
                        }
                        onClicked: window.visibility = window.visibility === Window.Maximized
                                                       ? Window.Windowed : Window.Maximized
                    }
                    Button {
                        text: "X"
                        implicitWidth: 34
                        implicitHeight: 32
                        ToolTip.visible: hovered
                        ToolTip.text: "Close MOKO Settings"
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#DCEBFA" : "#FFFFFF"
                            border.color: "#C6D7E5"
                        }
                        onClicked: window.close()
                    }
                }

                ListView {
                    id: sectionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: window.sections
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
                            color: "#182536"
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
                                              : parent.hovered ? "#F5FAFF" : "#FFFFFF"
                        border.color: parent.checked ? "#8EB6F8" : "#C7D9EA"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#1F3247"
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
                        color: parent.down ? "#D4E5FA" : parent.hovered ? "#F5FAFF" : "#FFFFFF"
                        border.color: "#C7D9EA"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#1F3247"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: mokoSettings.refresh()
                }
                Text {
                    Layout.fillWidth: true
                    text: "Updated " + mokoSettings.refreshedAt
                    color: "#718294"
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#FFFFFF"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text {
                        text: mokoSettings.sectionTitle(window.selectedSection)
                        color: "#111923"
                        font.pixelSize: 28
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: mokoSettings.sectionDescription(window.selectedSection)
                        color: "#6A7A8B"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                Rectangle {
                    visible: window.selectedSection === "date-time"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 250
                    radius: 8
                    color: "#F7FAFD"
                    border.color: "#DCE7EF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: mokoSettings.localDateTime
                            color: "#16263B"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Time zone"; color: "#40536A"; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: timeZoneCombo
                                Layout.preferredWidth: Math.min(340, parent.width * .58)
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
                            Text { Layout.fillWidth: true; text: "Set time automatically"; color: "#40536A"; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.automaticTime
                                enabled: mokoSettings.automaticTimeAvailable
                                onToggled: mokoSettings.setAutomaticTime(checked)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "24-hour clock"; color: "#40536A"; font.pixelSize: 12 }
                            Switch {
                                checked: mokoSettings.twentyFourHour
                                onToggled: mokoSettings.twentyFourHour = checked
                            }
                        }
                    }
                }

                ListView {
                    id: settingRows
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.selectedSection !== "date-time"
                    model: window.currentRows
                    spacing: 8
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property var modelData
                        width: settingRows.width
                        height: Math.max(66, rowContent.implicitHeight + 24)
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
                                Layout.preferredWidth: Math.min(330, settingRows.width * .42)
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
