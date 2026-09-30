#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QtWebEngine>

#include "bridge.h"

int main(int argc, char *argv[])
{
    // QtWebEngine dentro del confinamiento AppArmor de Ubuntu Touch:
    // el sandbox de Chromium no puede arrancar, AppArmor ya aísla la app.
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

    // Menos procesos de render = menos RAM en el teléfono.
    // Si ya definiste tus propios flags, se respetan y se añaden los nuestros.
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!flags.contains("--renderer-process-limit"))
        flags += " --renderer-process-limit=2";
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngine::initialize();

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("instagrambridge.aprilpixelrain"));
    app.setOrganizationName(QStringLiteral("instagrambridge.aprilpixelrain"));

    // Enlace pasado como argumento (Exec=instagrambridge %u)
    QString launchUrl;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i).startsWith(QLatin1String("http"))) {
            launchUrl = args.at(i);
            break;
        }
    }

    InstagramBridge bridge(launchUrl);

    QQuickView view;
    view.setTitle(QStringLiteral("IG Bridge"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.engine()->rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    QObject::connect(view.engine(), &QQmlEngine::quit, &app, &QGuiApplication::quit);

    QString qmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml/Main.qml");
    if (!QFileInfo::exists(qmlPath))
        qmlPath = QStringLiteral("qml/Main.qml"); // ejecución desde la carpeta del proyecto

    view.setSource(QUrl::fromLocalFile(QFileInfo(qmlPath).absoluteFilePath()));
    if (view.status() == QQuickView::Error)
        return 1;

    view.show();
    return app.exec();
}
