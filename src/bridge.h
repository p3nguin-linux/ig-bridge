#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

class InstagramBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userAgent READ userAgent CONSTANT)
    Q_PROPERTY(QUrl startUrl READ startUrl CONSTANT)
    Q_PROPERTY(QUrl homeUrl READ homeUrl CONSTANT)
    Q_PROPERTY(QString injectedScript READ injectedScript CONSTANT)
    Q_PROPERTY(QString startService READ startService CONSTANT)

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

    QString startService() const;
    Q_INVOKABLE QUrl homeUrlFor(const QString &service) const;
    Q_INVOKABLE QUrl startUrlFor(const QString &service) const;
    Q_INVOKABLE void rememberUrlFor(const QString &service, const QUrl &url) const;
    Q_INVOKABLE void rememberService(const QString &service) const;
    Q_INVOKABLE QString serviceForUrl(const QUrl &url) const;
    Q_INVOKABLE QVariantList menuItems(const QString &service) const;

private:
    QUrl m_launchUrl;
};
