import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtWebEngine

ApplicationWindow {
    id: window
    visible: true
    width: 1180
    height: 760
    minimumWidth: 720
    minimumHeight: 480
    title: currentTab >= 0 && tabs.get(currentTab).tabHome
           ? "New Tab - MOKO Browser"
           : currentView() ? currentView().title + " - MOKO Browser" : "MOKO Browser"
    color: "#F5F9FC"
    flags: Qt.Window | Qt.FramelessWindowHint

    property int currentTab: -1
    property string sidePanel: ""
    property bool findVisible: false
    property int findMatches: 0
    property int findActiveMatch: 0
    property bool validationDownloadStarted: false
    property string fileDialogPurpose: "open"
    property int editingQuickSite: -1

    function currentView() {
        return currentTab >= 0 ? webViews.itemAt(currentTab) : null
    }

    function createTab(target) {
        const value = target ? target.toString() : ""
        const home = value.length === 0 || value === "moko://home"
        const requested = home ? "about:blank" : value
        tabs.append({"initialUrl": requested, "tabTitle": "New Tab",
                     "tabUrl": home ? "moko://home" : requested,
                     "tabLoading": !home, "tabHome": home})
        currentTab = tabs.count - 1
        Qt.callLater(function() {
            const view = currentView()
            if (view)
                view.forceActiveFocus()
        })
    }

    function closeTab(index) {
        if (index < 0 || index >= tabs.count)
            return
        const closingCurrentTab = index === currentTab
        const closingBeforeCurrentTab = index < currentTab
        tabs.remove(index)
        if (tabs.count === 0) {
            createTab("")
        } else if (closingCurrentTab) {
            currentTab = Math.min(index, tabs.count - 1)
        } else if (closingBeforeCurrentTab) {
            currentTab--
        }
    }

    function navigateFromAddress() {
        const view = currentView()
        if (!view)
            return
        // Capture the user's text before leaving the Home tab. Changing
        // tabHome updates the address-field binding, which otherwise replaces
        // the pending query with the Home view's about:blank URL.
        const requestedUrl = mokoBrowser.urlFromInput(addressField.text)
        tabs.setProperty(currentTab, "tabHome", false)
        view.url = requestedUrl
        view.forceActiveFocus()
    }

    function openHome() {
        const view = currentView()
        if (!view)
            return
        view.stop()
        view.url = "about:blank"
        tabs.setProperty(currentTab, "tabHome", true)
        tabs.setProperty(currentTab, "tabUrl", "moko://home")
        tabs.setProperty(currentTab, "tabTitle", "New Tab")
        addressField.text = ""
    }

    function openQuickSite(url) {
        tabs.setProperty(currentTab, "tabHome", false)
        currentView().url = url
        currentView().forceActiveFocus()
    }

    function editQuickSite(index, site) {
        editingQuickSite = index
        quickSiteName.text = site ? site.label : ""
        quickSiteUrl.text = site ? site.url : "https://"
        quickSiteDialog.visible = true
        Qt.callLater(function() { quickSiteName.forceActiveFocus() })
    }

    function togglePanel(name) {
        sidePanel = sidePanel === name ? "" : name
    }

    function updateFind(backward) {
        const view = currentView()
        if (!view)
            return
        let options = backward ? WebEngineView.FindBackward : 0
        view.findText(findField.text, options, function(result) {
            findMatches = result.numberOfMatches
            findActiveMatch = result.activeMatch
        })
    }

    function chooseLocalFile() {
        fileDialogPurpose = "open"
        fileDialog.mode = "open"
        fileDialog.dialogTitle = "Open in MOKO Browser"
        fileDialog.suggestedName = ""
        fileDialog.openAt(mokoFilePicker.currentPath)
    }

    function chooseDownloadFolder() {
        fileDialogPurpose = "downloads"
        fileDialog.mode = "folder"
        fileDialog.dialogTitle = "Choose Download Folder"
        fileDialog.suggestedName = ""
        fileDialog.openAt(mokoBrowserDownloads.downloadDirectory)
    }

    component MokoToolButton: Button {
        implicitWidth: 36
        implicitHeight: 34
        font.pixelSize: 12
        hoverEnabled: true
        background: Rectangle {
            radius: 6
            color: parent.down ? "#D3E6F8" : parent.hovered ? "#E4F0FA" : "#F9FCFE"
            border.color: parent.activeFocus ? "#76A7F5" : "#C9DAE6"
        }
        contentItem: Text {
            text: parent.text
            color: parent.enabled ? "#203347" : "#93A2AF"
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component PanelButton: Button {
        implicitHeight: 30
        leftPadding: 11
        rightPadding: 11
        font.pixelSize: 10
        hoverEnabled: true
        background: Rectangle {
            radius: 6
            color: parent.down ? "#CFE2F5" : parent.hovered ? "#DFECF8" : "#F7FBFE"
            border.color: "#C8D9E6"
        }
        contentItem: Text {
            text: parent.text
            color: parent.enabled ? "#24405A" : "#94A2AE"
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ListModel { id: tabs }

    WebEngineProfile {
        id: browserProfile
        storageName: "MokoBrowser"
        downloadPath: mokoBrowserDownloads.downloadDirectory
        httpCacheType: WebEngineProfile.DiskHttpCache
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        onDownloadRequested: function(download) {
            if (mokoBrowserDownloads.acceptDownload(download))
                window.sidePanel = "downloads"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#EAF3F9"
            border.color: "#C9D9E5"

            MouseArea {
                anchors.fill: parent
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility = window.visibility === Window.Maximized
                                                    ? Window.Windowed : Window.Maximized
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 10
                spacing: 9

                Text {
                    text: "MOKO"
                    color: "#101820"
                    font.pixelSize: 19
                    font.weight: Font.Black
                }
                Text {
                    text: "BROWSER"
                    color: "#2F74E8"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                }
                Rectangle { width: 1; height: 18; color: "#C4D4E0" }
                Text {
                    Layout.fillWidth: true
                    text: currentTab >= 0 && tabs.get(currentTab).tabHome
                          ? "New Tab"
                          : currentView() && currentView().title.length > 0
                          ? currentView().title : "New Tab"
                    color: "#4B6073"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                MokoToolButton {
                    text: "-"
                    ToolTip.visible: hovered
                    ToolTip.text: "Minimize MOKO Browser"
                    onClicked: window.showMinimized()
                }
                MokoToolButton {
                    text: window.visibility === Window.Maximized ? "[]" : "[ ]"
                    implicitWidth: 40
                    ToolTip.visible: hovered
                    ToolTip.text: window.visibility === Window.Maximized ? "Restore MOKO Browser"
                                                                        : "Maximize MOKO Browser"
                    onClicked: window.visibility = window.visibility === Window.Maximized
                                                   ? Window.Windowed : Window.Maximized
                }
                MokoToolButton {
                    text: "X"
                    ToolTip.visible: hovered
                    ToolTip.text: "Close MOKO Browser"
                    onClicked: window.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#F4F8FB"
            border.color: "#D5E1EA"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 5

                ListView {
                    id: tabStrip
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: ListView.Horizontal
                    spacing: 4
                    clip: true
                    model: tabs
                    currentIndex: window.currentTab
                    delegate: Rectangle {
                        required property int index
                        required property string tabTitle
                        required property string tabUrl
                        required property bool tabLoading
                        width: Math.max(130, Math.min(220, (tabStrip.width - 8) / Math.max(1, Math.min(5, tabs.count))))
                        height: 34
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 6
                        color: index === window.currentTab ? "#FFFFFF"
                              : tabMouse.containsMouse ? "#E7F0F7" : "transparent"
                        border.color: index === window.currentTab ? "#BDD2E1" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 9
                            anchors.rightMargin: 5
                            spacing: 5
                            Rectangle {
                                implicitWidth: 7
                                implicitHeight: 7
                                radius: 4
                                color: tabLoading ? "#3D80ED" : tabUrl.startsWith("https://")
                                                       ? "#38A279" : "#91A0AD"
                            }
                            Text {
                                Layout.fillWidth: true
                                text: tabTitle.length > 0 ? tabTitle : "New Tab"
                                color: "#263B4E"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            MokoToolButton {
                                text: "X"
                                implicitWidth: 24
                                implicitHeight: 24
                                background: Rectangle {
                                    radius: 5
                                    color: parent.hovered ? "#E1EBF3" : "transparent"
                                }
                                onClicked: window.closeTab(index)
                            }
                        }
                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            anchors.rightMargin: 28
                            hoverEnabled: true
                            onClicked: window.currentTab = index
                        }
                    }
                }
                MokoToolButton {
                    text: "+"
                    ToolTip.visible: hovered
                    ToolTip.text: "New tab"
                    onClicked: window.createTab("")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#FFFFFF"
            border.color: "#D5E1EA"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7

                MokoToolButton {
                    text: "<"
                    enabled: currentView() && currentView().canGoBack
                    ToolTip.visible: hovered
                    ToolTip.text: "Back"
                    onClicked: currentView().goBack()
                }
                MokoToolButton {
                    text: ">"
                    enabled: currentView() && currentView().canGoForward
                    ToolTip.visible: hovered
                    ToolTip.text: "Forward"
                    onClicked: currentView().goForward()
                }
                MokoToolButton {
                    text: currentView() && currentView().loading ? "X" : "R"
                    ToolTip.visible: hovered
                    ToolTip.text: currentView() && currentView().loading ? "Stop" : "Reload"
                    onClicked: {
                        if (currentView().loading)
                            currentView().stop()
                        else
                            currentView().reload()
                    }
                }
                MokoToolButton {
                    text: "O"
                    ToolTip.visible: hovered
                    ToolTip.text: "Open local file"
                    onClicked: window.chooseLocalFile()
                }
                MokoToolButton {
                    text: "M"
                    ToolTip.visible: hovered
                    ToolTip.text: "MOKO home"
                    onClicked: window.openHome()
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 7
                    color: "#F5F9FC"
                    border.color: addressField.activeFocus ? "#6B9FEF" : "#C8D8E4"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 7
                        Text {
                            text: currentView() && currentView().url.toString().startsWith("https://")
                                  ? "HTTPS" : "WEB"
                            color: currentView() && currentView().url.toString().startsWith("https://")
                                   ? "#20805E" : "#728392"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                        }
                        Rectangle { width: 1; height: 18; color: "#D5E0E8" }
                        TextField {
                            id: addressField
                            Layout.fillWidth: true
                            text: window.currentTab >= 0 && tabs.get(window.currentTab).tabHome
                                  ? "" : currentView() ? currentView().url.toString() : ""
                            selectByMouse: true
                            placeholderText: "Search or enter address"
                            color: "#1D3042"
                            font.pixelSize: 11
                            background: Item {}
                            onAccepted: window.navigateFromAddress()
                        }
                    }
                }

                MokoToolButton {
                    text: "H"
                    checked: window.sidePanel === "history"
                    ToolTip.visible: hovered
                    ToolTip.text: "History"
                    onClicked: window.togglePanel("history")
                }
                MokoToolButton {
                    text: "D"
                    checked: window.sidePanel === "downloads"
                    ToolTip.visible: hovered
                    ToolTip.text: "Downloads"
                    onClicked: window.togglePanel("downloads")
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: currentView() && currentView().loading ? 3 : 0
                color: "#D8E7F4"
                Rectangle {
                    height: parent.height
                    width: parent.width * (currentView() ? currentView().loadProgress / 100 : 0)
                    color: "#397CF0"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: window.findVisible ? 42 : 0
            visible: window.findVisible
            color: "#EEF5FA"
            border.color: "#D1DFE9"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 7
                Text { text: "Find"; color: "#344B5E"; font.pixelSize: 10; font.weight: Font.DemiBold }
                TextField {
                    id: findField
                    Layout.fillWidth: true
                    Layout.maximumWidth: 420
                    implicitHeight: 30
                    selectByMouse: true
                    color: "#24394B"
                    font.pixelSize: 10
                    background: Rectangle { radius: 6; color: "#FFFFFF"; border.color: "#C5D6E3" }
                    onTextChanged: window.updateFind(false)
                    onAccepted: window.updateFind(false)
                }
                Text {
                    text: findField.text.length > 0 ? findActiveMatch + " / " + findMatches : ""
                    color: "#6D7F8E"
                    font.pixelSize: 9
                }
                PanelButton { text: "Previous"; onClicked: window.updateFind(true) }
                PanelButton { text: "Next"; onClicked: window.updateFind(false) }
                MokoToolButton {
                    text: "X"
                    implicitWidth: 30
                    implicitHeight: 30
                    onClicked: {
                        window.findVisible = false
                        findField.text = ""
                        if (currentView())
                            currentView().findText("")
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                anchors.fill: parent
                color: "#FFFFFF"
            }

            Repeater {
                id: webViews
                model: tabs
                delegate: WebEngineView {
                    required property int index
                    required property string initialUrl
                    required property bool tabHome
                    property real horizontalSwipeDelta: 0
                    anchors.fill: parent
                    visible: index === window.currentTab && !tabHome
                    enabled: visible
                    focus: visible
                    url: initialUrl
                    profile: browserProfile
                    settings.javascriptEnabled: true
                    settings.javascriptCanOpenWindows: true
                    settings.localContentCanAccessRemoteUrls: false
                    settings.localContentCanAccessFileUrls: false
                    settings.fullScreenSupportEnabled: true

                    // Qt WebEngine receives libinput's two-finger horizontal
                    // motion as a horizontal wheel event. Keep vertical
                    // scrolling untouched and turn only a deliberate swipe
                    // into browser history navigation.
                    WheelHandler {
                        acceptedDevices: PointerDevice.TouchPad
                        onWheel: function(event) {
                            if (!mokoBrowser.browserHistorySwipeEnabled())
                                return
                            if (Math.abs(event.angleDelta.x) <= Math.abs(event.angleDelta.y))
                                return
                            parent.horizontalSwipeDelta += event.angleDelta.x
                            if (Math.abs(parent.horizontalSwipeDelta) < 120)
                                return
                            const backwards = parent.horizontalSwipeDelta > 0
                            parent.horizontalSwipeDelta = 0
                            if (backwards && parent.canGoBack)
                                parent.goBack()
                            else if (!backwards && parent.canGoForward)
                                parent.goForward()
                            event.accepted = true
                        }
                    }

                    Component.onCompleted: {
                        if (mokoBrowser.smokeTest) {
                            loadHtml("<html><head><title>MOKO Browser Validation</title></head>" +
                                     "<body style='font-family:sans-serif;background:#f5f9fc;color:#17324a;padding:48px'>" +
                                     "<h1>MOKO Browser</h1><p>Qt WebEngine is ready.</p>" +
                                     "<script>document.body.dataset.moko='ready';</script></body></html>",
                                     "https://moko.invalid/validation")
                        }
                    }

                    onTitleChanged: {
                        if (!tabs.get(index).tabHome)
                            tabs.setProperty(index, "tabTitle", title.length > 0 ? title : "New Tab")
                    }
                    onUrlChanged: {
                        if (!tabs.get(index).tabHome)
                            tabs.setProperty(index, "tabUrl", url.toString())
                        if (index === window.currentTab && !tabs.get(index).tabHome
                                && !addressField.activeFocus)
                            addressField.text = url.toString()
                    }
                    onLoadingChanged: function(loadRequest) {
                        const active = loadRequest.status === WebEngineView.LoadStartedStatus
                                    || loadRequest.status === WebEngineView.LoadStoppedStatus
                        tabs.setProperty(index, "tabLoading", active && loading)
                        if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                            mokoBrowser.recordLoad(title, url, true, "")
                            runJavaScript("(() => 'moko-js-ready')()", function(result) {
                                mokoBrowser.reportJavaScript(url, result)
                                if (result === "moko-js-ready"
                                        && !window.validationDownloadStarted
                                        && mokoBrowser.validationDownloadUrl.toString().length > 0) {
                                    window.validationDownloadStarted = true
                                    url = mokoBrowser.validationDownloadUrl
                                }
                            })
                        } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                            mokoBrowser.recordLoad(title, url, false, loadRequest.errorString)
                        }
                    }
                    onNewWindowRequested: function(request) {
                        window.createTab(request.requestedUrl)
                    }
                    onRenderProcessTerminated: function(terminationStatus, exitCode) {
                        mokoBrowser.recordLoad(title, url, false,
                                               "Page process stopped (" + exitCode + ")")
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: window.currentTab >= 0 && tabs.get(window.currentTab).tabHome
                color: "#F7FBFF"

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(760, parent.width - 48)
                    spacing: 22

                    ColumnLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 2
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "M O K O"
                            color: "#101820"
                            font.pixelSize: 42
                            font.weight: Font.Black
                            font.letterSpacing: 0
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "B R O W S E R"
                            color: "#2F74E8"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0
                        }
                    }

                    TextField {
                        id: homeSearch
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        placeholderText: "Search the web or enter an address"
                        selectByMouse: true
                        color: "#1D3042"
                        font.pixelSize: 13
                        leftPadding: 18
                        rightPadding: 18
                        background: Rectangle {
                            radius: 8
                            color: "#FFFFFF"
                            border.width: homeSearch.activeFocus ? 2 : 1
                            border.color: homeSearch.activeFocus ? "#6B9FEF" : "#C8D8E4"
                        }
                        onAccepted: {
                            addressField.text = text
                            window.navigateFromAddress()
                            clear()
                        }
                    }

                    GridLayout {
                        id: quickSitesGrid
                        Layout.fillWidth: true
                        columns: width < 620 ? 2 : 4
                        columnSpacing: 10
                        rowSpacing: 10

                        Repeater {
                            model: mokoBrowser.quickSites
                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: (quickSitesGrid.width
                                                        - quickSitesGrid.columnSpacing
                                                          * (quickSitesGrid.columns - 1))
                                                       / quickSitesGrid.columns
                                Layout.preferredHeight: 86
                                radius: 8
                                color: siteMouse.containsMouse ? "#EAF4FD" : "#FFFFFF"
                                border.color: "#D3E2ED"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        color: "#20384E"
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.url.replace(/^https?:\/\//, "")
                                        color: "#708496"
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                    Item { Layout.fillHeight: true }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: "Edit"
                                            flat: true
                                            font.pixelSize: 9
                                            onClicked: window.editQuickSite(index, modelData)
                                        }
                                        Button {
                                            text: "Remove"
                                            flat: true
                                            font.pixelSize: 9
                                            onClicked: mokoBrowser.removeQuickSite(index)
                                        }
                                    }
                                }
                                MouseArea {
                                    id: siteMouse
                                    anchors.fill: parent
                                    anchors.bottomMargin: 30
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: window.openQuickSite(modelData.url)
                                }
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredWidth: (quickSitesGrid.width
                                                    - quickSitesGrid.columnSpacing
                                                      * (quickSitesGrid.columns - 1))
                                                   / quickSitesGrid.columns
                            Layout.preferredHeight: 86
                            text: "+  Add site"
                            onClicked: window.editQuickSite(-1, null)
                        }
                    }
                }
            }

            Rectangle {
                id: sideDrawer
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Math.min(390, parent.width * 0.44)
                visible: window.sidePanel.length > 0
                color: "#F7FAFC"
                border.color: "#C8D8E4"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        Layout.leftMargin: 16
                        Layout.rightMargin: 10
                        Text {
                            Layout.fillWidth: true
                            text: window.sidePanel === "history" ? "History" : "Downloads"
                            color: "#172D40"
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                        PanelButton {
                            visible: window.sidePanel === "history"
                            text: "Clear"
                            enabled: mokoBrowserHistory.count > 0
                            onClicked: mokoBrowserHistory.clear()
                        }
                        PanelButton {
                            visible: window.sidePanel === "downloads"
                            text: "Open folder"
                            onClicked: mokoBrowser.showDownloadsInFiles()
                        }
                        PanelButton {
                            visible: window.sidePanel === "downloads"
                            text: "Location"
                            onClicked: window.chooseDownloadFolder()
                        }
                        MokoToolButton {
                            text: "X"
                            implicitWidth: 30
                            implicitHeight: 30
                            onClicked: window.sidePanel = ""
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#D5E1E9" }

                    ListView {
                        id: historyList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: window.sidePanel === "history"
                        model: mokoBrowserHistory
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: Rectangle {
                            required property int index
                            required property string title
                            required property var entryUrl
                            required property string host
                            required property string visitedText
                            width: historyList.width
                            height: 68
                            color: historyMouse.containsMouse ? "#EAF3F9" : "transparent"
                            border.color: "#E0E9EF"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 15
                                anchors.rightMargin: 12
                                anchors.topMargin: 9
                                anchors.bottomMargin: 9
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: title
                                    color: "#243A4E"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { Layout.fillWidth: true; text: host; color: "#678092"; font.pixelSize: 9; elide: Text.ElideRight }
                                    Text { text: visitedText; color: "#82919D"; font.pixelSize: 8 }
                                }
                            }
                            MouseArea {
                                id: historyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    currentView().url = entryUrl
                                    window.sidePanel = ""
                                }
                            }
                        }
                    }

                    ListView {
                        id: downloadList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: window.sidePanel === "downloads"
                        model: mokoBrowserDownloads
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: Rectangle {
                            required property int index
                            required property string fileName
                            required property string statusText
                            required property real progress
                            required property bool canCancel
                            required property bool canOpen
                            width: downloadList.width
                            height: 92
                            color: "transparent"
                            border.color: "#DFE8EE"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6
                                Text {
                                    Layout.fillWidth: true
                                    text: fileName
                                    color: "#21384B"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    value: progress
                                    indeterminate: canCancel && progress <= 0
                                    background: Rectangle { implicitHeight: 5; radius: 2; color: "#D9E5ED" }
                                    contentItem: Item {
                                        implicitHeight: 5
                                        Rectangle {
                                            width: parent.width * parent.parent.visualPosition
                                            height: parent.height
                                            radius: 2
                                            color: "#3C7DEC"
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: statusText + (canCancel && progress > 0
                                              ? "  " + Math.round(progress * 100) + "%" : "")
                                        color: canOpen ? "#2B8162" : "#738594"
                                        font.pixelSize: 9
                                        font.weight: canOpen ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                    }
                                    PanelButton { text: "Cancel"; visible: canCancel; onClicked: mokoBrowserDownloads.cancel(index) }
                                    PanelButton { text: "Open"; visible: canOpen; onClicked: mokoBrowserDownloads.open(index) }
                                    PanelButton { text: "Show in Files"; visible: canOpen; onClicked: mokoBrowserDownloads.showInFiles(index) }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: (window.sidePanel === "history" && mokoBrowserHistory.count === 0)
                              || (window.sidePanel === "downloads" && mokoBrowserDownloads.count === 0)
                        text: window.sidePanel === "history" ? "No browsing history yet" : "No downloads yet"
                        color: "#7B8D9B"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: "#ECF3F8"
            border.color: "#D2DFE8"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Text {
                    Layout.fillWidth: true
                    text: mokoBrowser.statusMessage
                    color: "#63798A"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
                Text {
                    text: currentView() && currentView().url.toString().startsWith("https://")
                          ? "Secure connection" : ""
                    color: "#2B8162"
                    font.pixelSize: 9
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
        id: fileDialog
        picker: mokoFilePicker
        onPathAccepted: function(path) {
            if (window.fileDialogPurpose === "downloads") {
                mokoBrowserDownloads.setDownloadDirectory(path)
                return
            }
            const target = mokoBrowser.localFileUrl(path)
            if (target.toString().length > 0 && window.currentView()) {
                window.currentView().url = target
                window.currentView().forceActiveFocus()
            }
        }
    }

    Rectangle {
        id: quickSiteDialog
        anchors.fill: parent
        visible: false
        z: 200
        color: Qt.rgba(.10,.15,.21,.38)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(410, parent.width - 40)
            height: 230
            radius: 8
            color: "#F8FBFE"
            border.color: "#C7D8E6"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                Text {
                    text: window.editingQuickSite < 0 ? "Add quick site" : "Edit quick site"
                    color: "#172D40"
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }
                TextField {
                    id: quickSiteName
                    Layout.fillWidth: true
                    placeholderText: "Name"
                    selectByMouse: true
                }
                TextField {
                    id: quickSiteUrl
                    Layout.fillWidth: true
                    placeholderText: "https://example.com"
                    selectByMouse: true
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    PanelButton { text: "Cancel"; onClicked: quickSiteDialog.visible = false }
                    PanelButton {
                        text: "Save"
                        enabled: quickSiteName.text.trim().length > 0
                                 && quickSiteUrl.text.trim().length > 0
                        onClicked: {
                            const ok = window.editingQuickSite < 0
                                     ? mokoBrowser.addQuickSite(quickSiteName.text, quickSiteUrl.text)
                                     : mokoBrowser.updateQuickSite(window.editingQuickSite,
                                                                   quickSiteName.text,
                                                                   quickSiteUrl.text)
                            if (ok)
                                quickSiteDialog.visible = false
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: createTab(mokoBrowser.initialUrl)

    Shortcut { sequence: "Ctrl+L"; onActivated: { addressField.forceActiveFocus(); addressField.selectAll() } }
    Shortcut { sequence: "Ctrl+T"; onActivated: createTab("") }
    Shortcut { sequence: "Ctrl+W"; onActivated: closeTab(currentTab) }
    Shortcut { sequence: "Ctrl+R"; onActivated: if (currentView()) currentView().reload() }
    Shortcut { sequence: "Ctrl+F"; onActivated: { findVisible = true; findField.forceActiveFocus(); findField.selectAll() } }
    Shortcut { sequence: "Ctrl+O"; onActivated: window.chooseLocalFile() }
    Shortcut { sequence: "Alt+Left"; onActivated: if (currentView() && currentView().canGoBack) currentView().goBack() }
    Shortcut { sequence: "Alt+Right"; onActivated: if (currentView() && currentView().canGoForward) currentView().goForward() }
    Shortcut { sequence: "Ctrl+Shift+H"; onActivated: togglePanel("history") }
    Shortcut { sequence: "Ctrl+Shift+J"; onActivated: togglePanel("downloads") }
    Shortcut { sequence: "Ctrl+Shift+O"; onActivated: mokoBrowser.showDownloadsInFiles() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Meta+M"; onActivated: window.showMinimized() }
    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
                                       ? Window.Windowed : Window.FullScreen
    }
}
