import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    width: 1120
    height: 740
    minimumWidth: 860
    minimumHeight: 580
    title: "MOKO Hardware Diagnostics"
    color: "#F7FBFF"
    flags: Qt.Window | Qt.FramelessWindowHint

    property var sections: mokoHardware.sectionIds()
    property string selectedSection: sections.length ? sections[0] : "system"
    property bool developerMode: false
    property string exportFormat: "json"
    property var currentRows: []
    property int dataRevision: 0

    function sectionTitle(sectionId) {
        dataRevision
        return mokoHardware.sectionTitle(sectionId)
    }

    function sectionStatus(sectionId) {
        dataRevision
        return mokoHardware.sectionStatus(sectionId)
    }

    function sectionSummary(sectionId) {
        dataRevision
        return mokoHardware.sectionSummary(sectionId)
    }

    function statusColor(status) {
        if (status === "SUPPORTED") return "#1E8A62"
        if (status === "PARTIAL") return "#B56A18"
        if (status === "UNSUPPORTED") return "#B73E49"
        return "#637488"
    }

    function displayStatus(sectionId) {
        dataRevision
        const display = mokoHardware.sectionDisplayStatus(sectionId)
        return developerMode ? display + " / " + sectionStatus(sectionId) : display
    }

    function overallDisplayStatus() {
        dataRevision
        return developerMode ? mokoHardware.overallDisplayStatus + " / " + mokoHardware.overallStatus
                             : mokoHardware.overallDisplayStatus
    }

    function refreshRows() {
        const rows = mokoHardware.rows(selectedSection)
        currentRows = developerMode ? rows : rows.filter(function(row) { return !row.technical })
    }

    function selectSection(sectionId) {
        selectedSection = sectionId
        refreshRows()
    }

    function chooseReportPath(format) {
        exportFormat = format
        reportDialog.mode = "save"
        reportDialog.dialogTitle = format === "json" ? "Save JSON Hardware Report"
                                                     : "Save Text Hardware Report"
        reportDialog.suggestedName = format === "json" ? "moko-hardware-report.json"
                                                        : "moko-hardware-report.txt"
        reportDialog.openAt(mokoHardware.exportDirectory)
    }

    onDeveloperModeChanged: refreshRows()
    Component.onCompleted: refreshRows()

    Connections {
        target: mokoHardware
        function onDataChanged() {
            window.dataRevision += 1
            window.sections = mokoHardware.sectionIds()
            window.refreshRows()
        }
    }

    component HeaderButton: Button {
        id: headerButton
        implicitHeight: 36
        leftPadding: 13
        rightPadding: 13
        hoverEnabled: true
        background: Rectangle {
            radius: 6
            color: headerButton.down ? "#D5E7F7"
                                     : headerButton.hovered ? "#E9F3FA" : "#F8FBFD"
            border.color: "#C7D8E4"
        }
        contentItem: Text {
            text: headerButton.text
            color: "#203447"
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            color: "#FFFFFF"
            border.color: "#D7E4EE"

            MouseArea {
                anchors.fill: parent
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 16
                spacing: 12

                ColumnLayout {
                    spacing: 1
                    Text { text: "MOKO"; color: "#10151C"; font.pixelSize: 23; font.weight: Font.Black }
                    Text { text: "HARDWARE DIAGNOSTICS"; color: "#3978F6"; font.pixelSize: 10; font.weight: Font.Bold }
                }
                Text {
                    Layout.leftMargin: 12
                    text: "Compatibility: " + window.overallDisplayStatus()
                    color: "#52687B"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                HeaderButton {
                    text: window.developerMode ? "Advanced: On" : "Advanced"
                    checkable: true
                    checked: window.developerMode
                    onClicked: window.developerMode = checked
                }
                HeaderButton {
                    text: "Refresh"
                    ToolTip.visible: hovered
                    ToolTip.text: "Run the read-only hardware scan again"
                    onClicked: mokoHardware.refresh()
                }
                HeaderButton {
                    text: "Export JSON"
                    ToolTip.visible: hovered
                    ToolTip.text: "Export moko-hardware-report.json"
                    onClicked: window.chooseReportPath("json")
                }
                HeaderButton {
                    text: "Export Text"
                    ToolTip.visible: hovered
                    ToolTip.text: "Export moko-hardware-report.txt"
                    onClicked: window.chooseReportPath("txt")
                }
                HeaderButton {
                    text: "-"
                    implicitWidth: 36
                    ToolTip.visible: hovered
                    ToolTip.text: "Minimize Hardware Diagnostics"
                    onClicked: window.showMinimized()
                }
                HeaderButton {
                    text: window.visibility === Window.Maximized ? "[]" : "[ ]"
                    implicitWidth: 38
                    ToolTip.visible: hovered
                    ToolTip.text: window.visibility === Window.Maximized ? "Restore Hardware Diagnostics"
                                                                        : "Maximize Hardware Diagnostics"
                    onClicked: window.visibility = window.visibility === Window.Maximized
                                                   ? Window.Windowed : Window.Maximized
                }
                HeaderButton {
                    text: "X"
                    implicitWidth: 36
                    ToolTip.visible: hovered
                    ToolTip.text: "Close Hardware Diagnostics"
                    onClicked: window.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                color: "#EAF4FC"
                border.color: "#D2E1EC"

                ListView {
                    id: sectionList
                    anchors.fill: parent
                    anchors.margins: 14
                    model: window.sections
                    spacing: 4
                    clip: true
                    currentIndex: 0
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: sectionList.width
                        height: 48
                        radius: 7
                        color: window.selectedSection === modelData ? "#D9E9FF"
                                                                     : sectionMouse.containsMouse ? "#F5FAFF" : "transparent"
                        border.color: window.selectedSection === modelData ? "#AFCBFA" : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 10
                            Text {
                                Layout.fillWidth: true
                                text: window.sectionTitle(modelData)
                                color: "#182536"
                                font.pixelSize: 12
                                font.weight: window.selectedSection === modelData ? Font.DemiBold : Font.Normal
                            }
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: window.statusColor(window.sectionStatus(modelData))
                            }
                            Text {
                                text: window.displayStatus(modelData)
                                color: "#5F7182"
                                font.pixelSize: 9
                                font.weight: Font.Medium
                            }
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
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#FFFFFF"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 26
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: window.sectionTitle(window.selectedSection)
                                color: "#111923"
                                font.pixelSize: 27
                                font.weight: Font.Bold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: window.sectionSummary(window.selectedSection)
                                color: "#687A8C"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 9
                                height: 9
                                radius: 5
                                color: window.statusColor(window.sectionStatus(window.selectedSection))
                            }
                            Text {
                                text: window.displayStatus(window.selectedSection)
                                color: "#586D7F"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#E1E9F0" }

                    ListView {
                        id: evidenceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: window.currentRows
                        spacing: 0
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: evidenceList.width
                            height: Math.max(64, evidenceContent.implicitHeight + 22)
                            color: index % 2 ? "#FAFCFE" : "#FFFFFF"
                            border.color: "#E5ECF2"
                            RowLayout {
                                id: evidenceContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 18
                                Text {
                                    Layout.preferredWidth: Math.min(220, evidenceList.width * .31)
                                    text: modelData.label
                                    color: modelData.available ? "#28384A" : "#84909C"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.value
                                        color: modelData.available ? "#14243A" : "#8794A1"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        wrapMode: Text.WrapAnywhere
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: window.developerMode
                                                 && modelData.evidence && modelData.evidence.length > 0
                                        text: modelData.evidence || ""
                                        color: "#758698"
                                        font.pixelSize: 9
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 4
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: 7
                        color: "#ECF5FF"
                        border.color: "#D3E5F6"
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: window.developerMode
                                  ? mokoHardware.statusMessage + "  |  Reports: " + mokoHardware.exportDirectory
                                  : mokoHardware.statusMessage
                            color: "#567087"
                            font.pixelSize: 10
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                        }
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

    MokoFileDialog {
        id: reportDialog
        picker: mokoFilePicker
        onPathAccepted: function(path) {
            if (window.exportFormat === "json")
                mokoHardware.exportJsonToPath(path)
            else
                mokoHardware.exportTextToPath(path)
        }
    }

    Shortcut { sequence: "Ctrl+E"; onActivated: mokoHardware.exportAll() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
                                       ? Window.Windowed : Window.FullScreen
    }
}
