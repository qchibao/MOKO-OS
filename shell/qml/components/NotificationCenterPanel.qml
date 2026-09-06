import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GlassPanel {
    id: root
    property var notificationModel
    width: Math.min(390, Math.max(330, parent ? parent.width * 0.30 : 390))
    height: Math.min(610, Math.max(430, parent ? parent.height - 96 : 610))
    radius: 18
    glassOpacity: 0.94
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: "Notifications"
                color: "#142235"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            MokoActionButton {
                text: "Clear"
                enabled: root.notificationModel && root.notificationModel.count > 0
                onClicked: root.notificationModel.clearAll()
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.notificationModel && root.notificationModel.unreadCount > 0
                  ? root.notificationModel.unreadCount + " unread" : "You're all caught up"
            color: "#6D7D8E"
            font.pixelSize: 11
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#D6E3ED"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 8
                visible: !root.notificationModel || root.notificationModel.count === 0
                MokoGlyph {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 42
                    height: 42
                    kind: "notification"
                    primary: "#7C91A7"
                    secondary: "#B9C9D7"
                }
                Text {
                    width: parent.width
                    text: "No notifications"
                    color: "#536577"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ListView {
                id: notificationList
                anchors.fill: parent
                model: root.notificationModel
                spacing: 7
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                visible: root.notificationModel && root.notificationModel.count > 0

                delegate: Rectangle {
                    id: notificationRow
                    required property int index
                    required property string applicationName
                    required property string summary
                    required property string body
                    required property string createdText
                    required property bool unread
                    required property bool active
                    required property var actions
                    width: notificationList.width
                    height: notificationContent.implicitHeight + 24
                    radius: 8
                    color: unread ? "#EAF3FF" : "#F9FCFE"
                    border.color: unread ? "#B7D1F5" : "#D9E4EC"

                    RowLayout {
                        id: notificationContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 10

                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            width: 30
                            height: 30
                            radius: 7
                            color: unread ? "#3978F6" : "#DCE7F0"
                            MokoGlyph {
                                anchors.centerIn: parent
                                width: 19
                                height: 19
                                kind: "notification"
                                primary: unread ? "#FFFFFF" : "#647789"
                                secondary: primary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: notificationRow.applicationName
                                    color: "#718193"
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: notificationRow.createdText
                                    color: "#8A98A7"
                                    font.pixelSize: 9
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: notificationRow.summary
                                color: "#1E2F42"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: notificationRow.body
                                color: "#56687A"
                                font.pixelSize: 10
                                wrapMode: Text.Wrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: notificationRow.actions.length > 0
                                Repeater {
                                    model: notificationRow.actions
                                    delegate: MokoActionButton {
                                        required property var modelData
                                        text: modelData.label || "Open"
                                        onClicked: root.notificationModel.invokeAction(
                                                       notificationRow.index,
                                                       modelData.key || "")
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        ToolButton {
                            Layout.alignment: Qt.AlignTop
                            text: "X"
                            implicitWidth: 28
                            implicitHeight: 28
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.text: "Dismiss notification"
                            background: Rectangle {
                                radius: 6
                                color: parent.down ? "#D7E4EE"
                                                   : parent.hovered ? "#E8F1F7" : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#657789"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.notificationModel.dismiss(notificationRow.index)
                        }
                    }
                }
            }
        }
    }
}
