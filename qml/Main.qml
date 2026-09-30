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

    Page {
        id: page
        anchors.fill: parent
        header: PageHeader { visible: false; height: 0 }

        WebView {
            id: web
            anchors.fill: parent
            url: bridge.startUrl
            focus: true

            profile: WebEngineProfile {
                storageName: "instagram"
                httpUserAgent: bridge.userAgent
                persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
                httpCacheType: WebEngineProfile.DiskHttpCache

                onDownloadRequested: {
                    download.path = bridge.downloadPath(download.path)
                    download.accept()
                }
            }

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

            // Solo se navega dentro de la app por dominios permitidos;
            // todo lo demás se abre fuera.
            onNavigationRequested: {
                if (request.isMainFrame && !bridge.isAllowedHost(request.url)) {
                    request.action = WebEngineNavigationRequest.IgnoreRequest
                    bridge.openExternally(request.url)
                }
            }

            onNewViewRequested: {
                var target = request.requestedUrl
                if (bridge.isAllowedHost(target))
                    web.url = target
                else
                    bridge.openExternally(target)
            }

            // Cámara y micrófono (historias, reels, mensajes de voz)
            onFeaturePermissionRequested: {
                var media = feature === WebEngineView.MediaAudioCapture
                         || feature === WebEngineView.MediaVideoCapture
                         || feature === WebEngineView.MediaAudioVideoCapture
                web.grantFeaturePermission(securityOrigin, feature,
                                           media && bridge.isAllowedHost(securityOrigin))
            }

            onLoadingChanged: {
                if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus)
                    bridge.rememberUrl(web.url)
            }

            // Botón/gesto atrás: retrocede en el historial; si no hay, sale
            Keys.onBackPressed: {
                if (web.canGoBack)
                    web.goBack()
                else
                    Qt.quit()
            }
        }

        // Barra de progreso fina arriba
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            height: units.dp(3)
            width: parent.width * (web.loadProgress / 100)
            color: "#E1306C"
            visible: web.loading && web.loadProgress < 100
        }

        // Pantalla de error / sin conexión
        Item {
            id: errorView
            anchors.fill: parent
            visible: false
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
                    text: i18n.tr("No se pudo cargar Instagram.\nRevisa tu conexión.")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: i18n.tr("Reintentar")
                    color: "#E1306C"
                    onClicked: {
                        errorView.visible = false
                        web.reload()
                    }
                }
            }
        }

        Connections {
            target: web
            onLoadingChanged: {
                if (loadRequest.status === WebEngineLoadRequest.LoadFailedStatus)
                    errorView.visible = true
                else if (loadRequest.status === WebEngineLoadRequest.LoadSucceededStatus)
                    errorView.visible = false
            }
        }
    }
}
