#include "bridge.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {

const char kHomeUrl[] = "https://www.instagram.com/";

// Ajusta la versión de Chrome al motor real de tu Ubuntu Touch
// (UT 20.04 usa QtWebEngine 5.15 => Chromium 87).
const char kUserAgent[] =
    "Mozilla/5.0 (Linux; Android 11; Pixel 5) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/87.0.4280.144 Mobile Safari/537.36";

// Dominios que se cargan dentro de la app (Instagram + login con Facebook/Meta).
const QStringList &allowedDomains()
{
    static const QStringList domains = {
        QStringLiteral("instagram.com"),
        QStringLiteral("cdninstagram.com"),
        QStringLiteral("fbcdn.net"),
        QStringLiteral("facebook.com"),
        QStringLiteral("facebook.net"),
        QStringLiteral("fbsbx.com"),
        QStringLiteral("meta.com"),
    };
    return domains;
}

bool hostMatches(const QString &host, const QString &domain)
{
    return host == domain || host.endsWith(QLatin1Char('.') + domain);
}

} // namespace

InstagramBridge::InstagramBridge(const QString &launchUrl, QObject *parent)
    : QObject(parent), m_launchUrl(launchUrl)
{
}

QString InstagramBridge::userAgent() const
{
    return QString::fromLatin1(kUserAgent);
}

QUrl InstagramBridge::homeUrl() const
{
    return QUrl(QString::fromLatin1(kHomeUrl));
}

QUrl InstagramBridge::startUrl() const
{
    // 1) Enlace con el que se abrió la app (si es de Instagram)
    if (m_launchUrl.isValid() && isAllowedHost(m_launchUrl))
        return m_launchUrl;

    // 2) Última página visitada
    QSettings settings;
    const QUrl last = settings.value(QStringLiteral("lastUrl")).toUrl();
    if (last.isValid() && isAllowedHost(last))
        return last;

    // 3) Inicio
    return homeUrl();
}

bool InstagramBridge::isAllowedHost(const QUrl &url) const
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("about") || scheme == QLatin1String("data")
        || scheme == QLatin1String("blob"))
        return true;
    if (scheme != QLatin1String("https"))
        return false;

    const QString host = url.host().toLower();
    if (host.isEmpty())
        return false;
    for (const QString &domain : allowedDomains()) {
        if (hostMatches(host, domain))
            return true;
    }
    return false;
}

void InstagramBridge::openExternally(const QUrl &url) const
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        QDesktopServices::openUrl(url);
}

void InstagramBridge::rememberUrl(const QUrl &url) const
{
    if (!isAllowedHost(url) || url.scheme().toLower() != QLatin1String("https"))
        return;
    QSettings settings;
    settings.setValue(QStringLiteral("lastUrl"), url);
}

QString InstagramBridge::downloadPath(const QString &suggestedPath) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);

    // Solo el nombre del archivo: evita rutas maliciosas tipo ../../
    const QFileInfo info(suggestedPath);
    QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    if (base.isEmpty())
        base = QStringLiteral("instagram");

    auto build = [&](int n) {
        QString name = base;
        if (n > 0)
            name += QStringLiteral("_%1").arg(n);
        if (!suffix.isEmpty())
            name += QLatin1Char('.') + suffix;
        return QDir(dir).filePath(name);
    };

    int counter = 0;
    QString candidate = build(counter);
    while (QFileInfo::exists(candidate) && counter < 1000)
        candidate = build(++counter);
    return candidate;
}

QString InstagramBridge::injectedScript() const
{
    // Ajustes mínimos y robustos (no dependen de clases CSS internas de Instagram,
    // que cambian a menudo).
    return QStringLiteral(R"JS(
(function () {
  try {
    var style = document.createElement('style');
    style.textContent =
      'html{-webkit-tap-highlight-color:transparent;}' +
      'body{overscroll-behavior-y:contain;}' +
      '*{-webkit-touch-callout:none;}';
    (document.head || document.documentElement).appendChild(style);

    var meta = document.querySelector('meta[name=viewport]');
    if (!meta) {
      meta = document.createElement('meta');
      meta.name = 'viewport';
      (document.head || document.documentElement).appendChild(meta);
    }
    meta.content = 'width=device-width, initial-scale=1, viewport-fit=cover';
  } catch (e) {}
})();
)JS");
}
