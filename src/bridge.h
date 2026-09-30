#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

// Puente C++ <-> QML. Expone a la interfaz:
//  - el User-Agent móvil y la URL de arranque,
//  - la lista blanca de dominios que pueden cargarse dentro de la app,
//  - apertura de enlaces externos,
//  - ruta segura para descargas,
//  - un script de ajustes de rendimiento/estética que se inyecta en la web.
class InstagramBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userAgent READ userAgent CONSTANT)
    Q_PROPERTY(QUrl startUrl READ startUrl CONSTANT)
    Q_PROPERTY(QUrl homeUrl READ homeUrl CONSTANT)
    Q_PROPERTY(QString injectedScript READ injectedScript CONSTANT)

public:
    explicit InstagramBridge(const QString &launchUrl = QString(), QObject *parent = nullptr);

    QString userAgent() const;
    QUrl startUrl() const;
    QUrl homeUrl() const;
    QString injectedScript() const;

    Q_INVOKABLE bool isAllowedHost(const QUrl &url) const;
    Q_INVOKABLE void openExternally(const QUrl &url) const;
    Q_INVOKABLE void rememberUrl(const QUrl &url) const;
    Q_INVOKABLE QString downloadPath(const QString &suggestedPath) const;

private:
    QUrl m_launchUrl;
};
