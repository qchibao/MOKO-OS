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

    property var sections: mokoHardware.sectionIds()
    property string selectedSection: sections.length ? sections[0] : "system"
    property var currentRows: mokoHardware.rows(selectedSection)
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

    function statusBackground(status) {
        if (status === "SUPPORTED") return "#E1F5EC"
        if (status === "PARTIAL") return "#FFF0D8"
        if (status === "UNSUPPORTED") return "#FBE4E7"
        return "#EAF0F5"
    }

    function selectSection(sectionId) {
        selectedSection = sectionId
        currentRows = mokoHardware.rows(sectionId)
    }

    Connections {
        target: mokoHardware
        function onDataChanged() {
            window.dataRevision += 1
            window.sections = mokoHardware.sectionIds()
            window.currentRows = mokoHardware.rows(window.selectedSection)
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
                Rectangle {
                    Layout.leftMargin: 12
                    implicitWidth: overallLabel.implicitWidth + 22
                    implicitHeight: 30
                    radius: 7
                    color: window.statusBackground(mokoHardware.overallStatus)
                    Text {
                        id: overallLabel
                        anchors.centerIn: parent
                        text: mokoHardware.overallStatus
                        color: window.statusColor(mokoHardware.overallStatus)
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Refresh"
                    ToolTip.visible: hovered
                    ToolTip.text: "Run the read-only hardware scan again"
                    onClicked: mokoHardware.refresh()
                }
                Button {
                    text: "Export JSON"
                    ToolTip.visible: hovered
                    ToolTip.text: "Export moko-hardware-report.json"
                    onClicked: mokoHardware.exportJson()
                }
                Button {
                    text: "Export Text"
                    ToolTip.visible: hovered
                    ToolTip.text: "Export moko-hardware-report.txt"
                    onClicked: mokoHardware.exportText()
                }
                Button {
                    text: "X"
                    implicitWidth: 36
                    ToolTip.visible: hovered
                    ToolTip.text: "Close Hardware Diagnostics"
                    onClicked: Qt.quit()
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
                                implicitWidth: 70
                                implicitHeight: 22
                                radius: 6
                                color: window.statusBackground(window.sectionStatus(modelData))
                                Text {
                                    anchors.centerIn: parent
                                    text: window.sectionStatus(modelData)
                                    color: window.statusColor(window.sectionStatus(modelData))
                                    font.pixelSize: 8
                                    font.weight: Font.Bold
                                }
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
                        Rectangle {
                            implicitWidth: sectionStatusLabel.implicitWidth + 24
                            implicitHeight: 32
                            radius: 7
                            color: window.statusBackground(window.sectionStatus(window.selectedSection))
                            Text {
                                id: sectionStatusLabel
                                anchors.centerIn: parent
                                text: window.sectionStatus(window.selectedSection)
                                color: window.statusColor(window.sectionStatus(window.selectedSection))
                                font.pixelSize: 10
                                font.weight: Font.Bold
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
                                        visible: modelData.evidence && modelData.evidence.length > 0
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
                            text: mokoHardware.statusMessage + "  |  Reports: " + mokoHardware.exportDirectory
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

    Shortcut { sequence: "Ctrl+E"; onActivated: mokoHardware.exportAll() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
}
