import QtQuick 2.12
import Lomiri.Components 1.3
import Morph.Web 0.1
import QtWebEngine 1.7

MainView {
    id: root
    applicationName: "instagrambridge.aprilpixelrain"
    automaticOrientation: true
    anchorToKeyboard: true
    backgroundColor: "#000000"

    width: units.gu(45)
    height: units.gu(75)

    property bool showToolbar: true
    property string currentService: bridge.startService
    readonly property bool isFacebook: currentService === "facebook"
    readonly property color serviceColor: isFacebook ? "#1877F2" : "#E1306C"
    readonly property var activeWeb: (isFacebook && fbLoader.item) ? fbLoader.item : web
    property bool menuOpen: false  
    property bool fabOpen: false    
    property bool igError: false
    property bool fbError: false
    property string pendingScript: ""
    property real targetCssWidth: 412
    property bool igFitted: false
    property bool fbFitted: false

    function fitZoom(view) {
        if (!view)
            return
        view.runJavaScript("window.innerWidth", function (w) {
            if (!w || w <= 0)
                return
            var current = view.zoomFactor
            var z = current * w / root.targetCssWidth
            z = Math.max(0.5, Math.min(3.0, z))
            if (Math.abs(z - current) / current > 0.05)
                view.zoomFactor = z
        })
    }

    function pauseMedia(view) {
        if (!view)
            return
        view.runJavaScript("document.querySelectorAll('video,audio').forEach(function(m){try{m.pause()}catch(e){}})")
    }

    onWidthChanged: fitTimer.restart()

    Timer {
        id: fitTimer
        interval: 400
        onTriggered: root.fitZoom(root.activeWeb)
    }

    function switchService(service) {
        if (service === currentService)
            return
        pauseMedia(activeWeb)
        if (service === "facebook")
            fbLoader.active = true
        currentService = service
        fabOpen = false
        bridge.rememberService(service)
        fitTimer.restart()
        Qt.callLater(function () { activeWeb.forceActiveFocus() })
    }

    Component.onCompleted: {
        if (isFacebook)
            fbLoader.active = true
    }

    function runMenuItem(item) {
        if (item.script && item.script !== "") {
            activeWeb.runJavaScript(item.script, function (ok) {
                if (!ok && item.url) {
                    // El botón no está en esta página: vamos al inicio y reintentamos
                    root.pendingScript = item.script
                    activeWeb.url = item.url
                }
            })
        } else if (item.url) {
            activeWeb.url = item.url
        }
        menuOpen = false
        fabOpen = false
    }

    Timer {
        id: menuCloseTimer
        interval: 380
        onTriggered: root.menuOpen = false
    }

    Timer {
        id: pendingTimer
        interval: 1800
        onTriggered: {
            if (root.pendingScript !== "") {
                var s = root.pendingScript
                root.pendingScript = ""
                root.activeWeb.runJavaScript(s)
            }
        }
    }

    Page {
        id: page
        anchors.fill: parent
        header: PageHeader { visible: false; height: 0 }

        WebEngineProfile {
            id: igProfile
            storageName: "instagram"
            httpUserAgent: bridge.userAgent
            persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
            httpCacheType: WebEngineProfile.DiskHttpCache
            httpCacheMaximumSize: 157286400   // 150 MB

            onDownloadRequested: {
                download.path = bridge.downloadPath(download.path)
                download.accept()
            }
        }

        WebEngineProfile {
            id: fbProfile
            storageName: "facebook"
            httpUserAgent: bridge.userAgent
            persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
            httpCacheType: WebEngineProfile.DiskHttpCache
            httpCacheMaximumSize: 157286400   // 150 MB

            onDownloadRequested: {
                download.path = bridge.downloadPath(download.path)
                download.accept()
            }
        }

        Rectangle {
            id: toolbar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: root.showToolbar ? units.gu(5) : 0
            visible: root.showToolbar
            color: "#121212"

            Row {
                anchors.fill: parent

                AbstractButton {
                    width: units.gu(6)
                    height: toolbar.height
                    enabled: root.activeWeb.canGoBack
                    opacity: enabled ? 1.0 : 0.35
                    onClicked: root.activeWeb.goBack()
                    Icon {
                        anchors.centerIn: parent
                        width: units.gu(2.4); height: width
                        name: "go-previous"
                        color: "#FFFFFF"
                    }
                }

                AbstractButton {
                    width: units.gu(6)
                    height: toolbar.height
                    onClicked: root.activeWeb.url = bridge.homeUrlFor(root.currentService)
                    Icon {
                        anchors.centerIn: parent
                        width: units.gu(2.4); height: width
                        name: "go-home"
                        color: "#FFFFFF"
                    }
                }

                AbstractButton {
                    width: units.gu(6)
                    height: toolbar.height
                    onClicked: root.activeWeb.reload()
                    Icon {
                        anchors.centerIn: parent
                        width: units.gu(2.4); height: width
                        name: "reload"
                        color: "#FFFFFF"
                    }
                }
            }
        }

        Row {
            id: toolbarRight
            anchors { top: toolbar.top; right: toolbar.right }
            height: toolbar.height
            visible: root.showToolbar

            AbstractButton {
                width: units.gu(6)
                height: toolbar.height
                onClicked: {
                    root.fabOpen = false
                    root.menuOpen = !root.menuOpen
                }
                Icon {
                    anchors.centerIn: parent
                    width: units.gu(2.4); height: width
                    name: "navigation-menu"
                    color: root.menuOpen ? root.serviceColor : "#FFFFFF"
                }
            }
        }

        // ---------- Web principal (Instagram) ----------
        WebView {
            id: web
            anchors {
                top: toolbar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            url: bridge.startUrlFor("instagram")
            focus: true
            opacity: root.isFacebook ? 0 : 1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
            profile: igProfile

            userScripts: [
                WebEngineScript {
                    name: "ig-bridge-tweaks"
                    sourceCode: bridge.injectedScript
                    injectionPoint: WebEngineScript.DocumentReady
                    worldId: WebEngineScript.MainWorld
                    runOnSubframes: false
                }
            ]

            settings.javascriptCanOpenWindows: true
            settings.localStorageEnabled: true
            settings.showScrollBars: false

            onNavigationRequested: {
                if (request.isMainFrame && !bridge.isAllowedHost(request.url)) {
                    request.action = WebEngineNavigationRequest.IgnoreRequest
                    bridge.openExternally(request.url)
                }
            }

            onNewViewRequested: {
                var target = request.requestedUrl.toString()
                var blank = (target === "" || target === "about:blank")
                if (blank || bridge.isAllowedHost(request.requestedUrl)) {
                    popupLoader.forFacebook = false
                    popupLoader.active = true
                    request.openIn(popupLoader.item.view)
                } else {
                    bridge.openExternally(request.requestedUrl)
                }
            }

            onFeaturePermissionRequested: {
                var media = feature === WebEngineView.MediaAudioCapture
                         || feature === WebEngineView.MediaVideoCapture
                         || feature === WebEngineView.MediaAudioVideoCapture
                web.grantFeaturePermission(securityOrigin, feature,
                                           media && bridge.isAllowedHost(securityOrigin))
            }

            onLoadingChanged: {
                if (loadRequest.status === WebEngineLoadRequest.LoadStartedStatus)
                    root.igFitted = false
                if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus) {
                    bridge.rememberUrlFor("instagram", web.url)
                    root.fitZoom(web)
                    if (root.pendingScript !== "" && !root.isFacebook)
                        pendingTimer.restart()
                }
            }

            onLoadProgressChanged: {
                if (web.loadProgress >= 60 && !root.igFitted) {
                    root.igFitted = true
                    root.fitZoom(web)
                }
            }

            Keys.onBackPressed: {
                if (popupLoader.active)
                    popupLoader.item.closePopup()
                else if (web.canGoBack)
                    web.goBack()
                else
                    Qt.quit()
            }

            Keys.onPressed: {
                if ((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Left) {
                    web.goBack()
                    event.accepted = true
                }
            }
        }

        Loader {
            id: fbLoader
            anchors {
                top: toolbar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            active: false
            opacity: root.isFacebook ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
            sourceComponent: fbComponent
        }

        Component {
            id: fbComponent

            WebView {
                id: fbWeb
                anchors.fill: parent
                url: bridge.startUrlFor("facebook")
                focus: true
                profile: fbProfile

                userScripts: [
                    WebEngineScript {
                        name: "fb-bridge-tweaks"
                        sourceCode: bridge.injectedScript
                        injectionPoint: WebEngineScript.DocumentReady
                        worldId: WebEngineScript.MainWorld
                        runOnSubframes: false
                    }
                ]

                settings.javascriptCanOpenWindows: true
                settings.localStorageEnabled: true
                settings.showScrollBars: false

                onNavigationRequested: {
                    if (!request.isMainFrame)
                        return
                    if (!bridge.isAllowedHost(request.url)) {
                        request.action = WebEngineNavigationRequest.IgnoreRequest
                        bridge.openExternally(request.url)
                    } else if (bridge.serviceForUrl(request.url) === "instagram") {
                        request.action = WebEngineNavigationRequest.IgnoreRequest
                        web.url = request.url
                        root.switchService("instagram")
                    }
                }

                onNewViewRequested: {
                    var target = request.requestedUrl.toString()
                    var blank = (target === "" || target === "about:blank")
                    if (!blank && bridge.serviceForUrl(request.requestedUrl) === "instagram") {
                        web.url = request.requestedUrl
                        root.switchService("instagram")
                    } else if (blank || bridge.isAllowedHost(request.requestedUrl)) {
                        popupLoader.forFacebook = true
                        popupLoader.active = true
                        request.openIn(popupLoader.item.view)
                    } else {
                        bridge.openExternally(request.requestedUrl)
                    }
                }

                onFeaturePermissionRequested: {
                    var media = feature === WebEngineView.MediaAudioCapture
                             || feature === WebEngineView.MediaVideoCapture
                             || feature === WebEngineView.MediaAudioVideoCapture
                    fbWeb.grantFeaturePermission(securityOrigin, feature,
                                                 media && bridge.isAllowedHost(securityOrigin))
                }

                onLoadingChanged: {
                    if (loadRequest.status === WebEngineLoadRequest.LoadStartedStatus) {
                        root.fbFitted = false
                    } else if (loadRequest.status === WebEngineLoadRequest.LoadFailedStatus) {
                        root.fbError = true
                    } else if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus) {
                        root.fbError = false
                        bridge.rememberUrlFor("facebook", fbWeb.url)
                        root.fitZoom(fbWeb)
                        if (root.pendingScript !== "" && root.isFacebook)
                            pendingTimer.restart()
                    }
                }

                onLoadProgressChanged: {
                    if (fbWeb.loadProgress >= 60 && !root.fbFitted) {
                        root.fbFitted = true
                        root.fitZoom(fbWeb)
                    }
                }

                Keys.onBackPressed: {
                    if (popupLoader.active)
                        popupLoader.item.closePopup()
                    else if (fbWeb.canGoBack)
                        fbWeb.goBack()
                    else
                        Qt.quit()
                }

                Keys.onPressed: {
                    if ((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Left) {
                        fbWeb.goBack()
                        event.accepted = true
                    }
                }
            }
        }

        Rectangle {
            anchors.top: toolbar.bottom
            anchors.left: parent.left
            height: units.dp(3)
            width: parent.width * (root.activeWeb.loadProgress / 100)
            color: root.serviceColor
            visible: root.activeWeb.loading && root.activeWeb.loadProgress < 100
        }

        Item {
            id: errorView
            anchors {
                top: toolbar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            visible: root.isFacebook ? root.fbError : root.igError
            Rectangle { anchors.fill: parent; color: "#000000" }

            Column {
                anchors.centerIn: parent
                spacing: units.gu(2)
                width: parent.width - units.gu(6)

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#FFFFFF"
                    text: root.isFacebook ? i18n.tr("No se pudo cargar Facebook.\nRevisa tu conexión.")
                                      : i18n.tr("No se pudo cargar Instagram.\nRevisa tu conexión.")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: i18n.tr("Reintentar")
                    color: root.serviceColor
                    onClicked: {
                        root.igError = false
                        root.fbError = false
                        root.activeWeb.reload()
                    }
                }
            }
        }

        Connections {
            target: web
            onLoadingChanged: {
                if (loadRequest.status === WebEngineLoadRequest.LoadFailedStatus)
                    root.igError = true
                else if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus)
                    root.igError = false
            }
        }

        Item {
            id: menuLayer
            anchors {
                top: toolbar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            visible: menuPanel.opacity > 0
            z: 6

            // Tocar fuera del menú lo cierra
            MouseArea {
                anchors.fill: parent
                enabled: root.menuOpen
                onClicked: root.menuOpen = false
            }

            Rectangle {
                id: menuPanel
                readonly property real rowH: units.gu(5.5)
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: units.dp(2)
                    rightMargin: units.gu(1)
                }
                width: units.gu(26)
                height: units.gu(4.5) + rowH * 2 + units.gu(1)
                radius: units.gu(1)
                color: "#1C1C1C"
                border.width: units.dp(1)
                border.color: root.serviceColor
                Behavior on border.color { ColorAnimation { duration: 250 } }

                transformOrigin: Item.TopRight
                opacity: root.menuOpen ? 1 : 0
                scale: root.menuOpen ? 1 : 0.85
                Behavior on opacity { NumberAnimation { duration: 180 } }
                Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

                Label {
                    id: menuTitle
                    anchors {
                        top: parent.top
                        left: parent.left
                        leftMargin: units.gu(2)
                    }
                    height: units.gu(4.5)
                    verticalAlignment: Text.AlignVCenter
                    text: "Cambiar de web"
                    fontSize: "small"
                    color: "#BBBBBB"
                }

                Item {
                    id: switcher
                    anchors {
                        top: menuTitle.bottom
                        left: parent.left
                        right: parent.right
                        margins: units.gu(0.5)
                    }
                    height: menuPanel.rowH * 2

                    Rectangle {
                        width: parent.width
                        height: menuPanel.rowH
                        radius: units.gu(0.8)
                        color: "#303030"
                        border.width: units.dp(1)
                        border.color: root.serviceColor
                        y: root.isFacebook ? menuPanel.rowH : 0
                        Behavior on y { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 250 } }
                    }

                    Column {
                        anchors.fill: parent

                        Repeater {
                            model: [
                                { key: "instagram", name: "Instagram", tint: "#E1306C" },
                                { key: "facebook",  name: "Facebook",  tint: "#1877F2" }
                            ]

                            delegate: AbstractButton {
                                width: switcher.width
                                height: menuPanel.rowH
                                onClicked: {
                                    root.switchService(modelData.key)
                                    menuCloseTimer.restart()
                                }

                                Row {
                                    anchors {
                                        fill: parent
                                        leftMargin: units.gu(1.5)
                                    }
                                    spacing: units.gu(1.5)

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: units.gu(2); height: width; radius: width / 2
                                        color: modelData.tint
                                    }
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name
                                        font.bold: root.currentService === modelData.key
                                        color: root.currentService === modelData.key ? "#FFFFFF" : "#999999"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: fabLayer
            anchors {
                top: toolbar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            z: 4
            // El botón "+" solo existe en Instagram; en Facebook no se muestra
            visible: !root.isFacebook

            readonly property real fabSize: units.gu(6.5)
            readonly property real itemSize: units.gu(5.5)
            readonly property real ringRadius: units.gu(17)
            readonly property real cx: width / 2
            readonly property real cy: height - units.gu(2.5) - fabSize / 2

            Rectangle {
                anchors.fill: parent
                color: "#000000"
                opacity: root.fabOpen ? 0.6 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
            MouseArea {
                anchors.fill: parent
                enabled: root.fabOpen
                onClicked: root.fabOpen = false
            }

            Repeater {
                id: fabItems
                model: bridge.menuItems(root.currentService)

                delegate: Item {
                    id: fabItem
                    // Reparto en semicírculo sobre el botón (de 170° a 10°)
                    readonly property real angle:
                        Math.PI * (170 - index * (160 / Math.max(1, fabItems.count - 1))) / 180

                    width: fabLayer.itemSize + units.gu(4)
                    height: fabLayer.itemSize + units.gu(3.5)
                    x: (root.fabOpen ? fabLayer.cx + fabLayer.ringRadius * Math.cos(angle)
                                     : fabLayer.cx) - width / 2
                    y: (root.fabOpen ? fabLayer.cy - fabLayer.ringRadius * Math.sin(angle)
                                     : fabLayer.cy) - fabLayer.itemSize / 2
                    opacity: root.fabOpen ? 1 : 0
                    scale: root.fabOpen ? 1 : 0.2
                    visible: opacity > 0

                    Behavior on x { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }
                    Behavior on y { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                    Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }

                    Column {
                        width: parent.width
                        spacing: units.gu(0.4)

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: fabLayer.itemSize; height: width; radius: width / 2
                            color: "#262626"
                            border.width: units.dp(2)
                            border.color: root.serviceColor

                            Icon {
                                anchors.centerIn: parent
                                width: units.gu(2.6); height: width
                                name: modelData.icon
                                color: "#FFFFFF"
                            }
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            fontSize: "x-small"
                            color: "#FFFFFF"
                            text: modelData.label
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: root.fabOpen
                        onClicked: root.runMenuItem(modelData)
                    }
                }
            }

            Rectangle {
                id: fabButton
                width: fabLayer.fabSize; height: width; radius: width / 2
                x: fabLayer.cx - width / 2
                y: fabLayer.cy - height / 2
                color: root.serviceColor
                border.width: units.dp(1)
                border.color: "#44FFFFFF"
                rotation: root.fabOpen ? 45 : 0
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on rotation { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

                Icon {
                    anchors.centerIn: parent
                    width: units.gu(3); height: width
                    name: "add"
                    color: "#FFFFFF"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.menuOpen = false
                        root.fabOpen = !root.fabOpen
                    }
                }
            }
        }

        Loader {
            id: popupLoader
            anchors.fill: parent
            active: false
            z: 10
            // true = el popup viene de Facebook (usa su propia sesión)
            property bool forFacebook: false
            sourceComponent: popupComponent
        }

        Component {
            id: popupComponent

            Rectangle {
                id: popupRoot
                color: "#000000"
                property alias view: popupView

                function closePopup() {
                    Qt.callLater(function () { popupLoader.active = false })
                }

                Rectangle {
                    id: popupBar
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: units.gu(5)
                    color: "#121212"

                    AbstractButton {
                        id: popupClose
                        anchors.left: parent.left
                        width: units.gu(6)
                        height: parent.height
                        onClicked: popupRoot.closePopup()
                        Icon {
                            anchors.centerIn: parent
                            width: units.gu(2.4); height: width
                            name: "close"
                            color: "#FFFFFF"
                        }
                    }

                    Label {
                        anchors {
                            left: popupClose.right
                            right: parent.right
                            rightMargin: units.gu(2)
                            verticalCenter: parent.verticalCenter
                        }
                        elide: Text.ElideRight
                        color: "#BBBBBB"
                        text: popupView.title
                    }
                }

                WebEngineView {
                    id: popupView
                    anchors {
                        top: popupBar.bottom
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    profile: popupLoader.forFacebook ? fbProfile : igProfile
                    focus: true

                    onWindowCloseRequested: popupRoot.closePopup()

                    onLoadingChanged: {
                        if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus)
                            root.fitZoom(popupView)
                    }

                    onNavigationRequested: {
                        if (request.isMainFrame && !bridge.isAllowedHost(request.url)) {
                            request.action = WebEngineNavigationRequest.IgnoreRequest
                            bridge.openExternally(request.url)
                        }
                    }

                    onNewViewRequested: {
                        if (bridge.isAllowedHost(request.requestedUrl))
                            request.openIn(popupView)
                        else
                            bridge.openExternally(request.requestedUrl)
                    }

                    Keys.onBackPressed: popupRoot.closePopup()
                }
            }
        }
    }
}
