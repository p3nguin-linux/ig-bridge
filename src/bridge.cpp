#include "bridge.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

namespace {

const char kHomeUrl[] = "https://www.instagram.com/";
const char kFbHomeUrl[] = "https://m.facebook.com/";

const char kUserAgent[] =
    "Mozilla/5.0 (Linux; Android 11; Pixel 5) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/87.0.4280.144 Mobile Safari/537.36";

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

const char kIgCreateScript[] = R"JS(
(function () {
  try {
    var re = /^(new post|create|nueva publicaci[oó]n|crear)$/i;
    var nodes = document.querySelectorAll('[aria-label]');
    for (var i = 0; i < nodes.length; i++) {
      var l = (nodes[i].getAttribute('aria-label') || '').trim();
      if (re.test(l)) {
        var t = nodes[i].closest('a,button,[role=button],[role=link]') || nodes[i];
        t.click();
        return true;
      }
    }
  } catch (e) {}
  return false;
})()
)JS";

const char kFbCreateScript[] = R"JS(
(function () {
  try {
    var re = /(qu[eé] est[aá]s pensando|what'?s on your mind|crear publicaci[oó]n|create (a )?post)/i;
    var nodes = document.querySelectorAll('a,button,[role=button],[aria-label]');
    for (var i = 0; i < nodes.length; i++) {
      var txt = (nodes[i].getAttribute('aria-label') || nodes[i].innerText || '').trim();
      if (txt.length < 80 && re.test(txt)) {
        nodes[i].click();
        return true;
      }
    }
  } catch (e) {}
  return false;
})()
)JS";

QVariantMap menuItem(const QString &id, const QString &label, const QString &icon,
                     const QString &url, const QString &script = QString())
{
    QVariantMap m;
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("label"), label);
    m.insert(QStringLiteral("icon"), icon);
    m.insert(QStringLiteral("url"), url);
    m.insert(QStringLiteral("script"), script);
    return m;
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
    if (m_launchUrl.isValid() && isAllowedHost(m_launchUrl))
        return m_launchUrl;

    QSettings settings;
    const QUrl last = settings.value(QStringLiteral("lastUrl")).toUrl();
    if (last.isValid() && isAllowedHost(last))
        return last;

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
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return;

    const QString host = url.host().toLower();
    static const QStringList stores = {
        QStringLiteral("play.google.com"), QStringLiteral("apps.apple.com"),
        QStringLiteral("itunes.apple.com"), QStringLiteral("onelink.me"),
    };
    for (const QString &s : stores) {
        if (hostMatches(host, s))
            return;
    }
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
    return QStringLiteral(R"JS(
(function () {
  try {
    var style = document.createElement('style');
    style.textContent =
      'html{-webkit-tap-highlight-color:transparent;}' +
      'body{overscroll-behavior-y:contain;}' +
      '*{-webkit-touch-callout:none;}' +
      // Toques sin retardo (quita la espera del doble toque) y texto sin reescalado
      'html{touch-action:manipulation;-webkit-text-size-adjust:100%;}' +
      // Efectos de desenfoque: muy caros de dibujar en el teléfono y casi no se notan
      '*{-webkit-backdrop-filter:none!important;backdrop-filter:none!important;}';
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

// Oculta las barras de navegación nativas (inferior/superior) de Instagram y
// Facebook: todas sus opciones ahora están en el botón "+" de la app.
// No depende de clases CSS: busca los enlaces de navegación y oculta la barra
// fija/pegajosa que los contiene.
(function () {
  try {
    var host = location.hostname;
    var isIg = /(^|\.)instagram\.com$/.test(host);
    var isFb = /(^|\.)facebook\.com$/.test(host);
    // Solo en Instagram: en Facebook se dejan sus barras de navegación originales
    if (!isIg) return;

    var selectors = isIg
      ? ['a[href^="/direct/"]', 'a[href^="/explore"]', 'a[href^="/reels"]',
         'a[href^="/accounts/activity"]']
      : ['a[href*="/notifications"]', 'a[href*="/friends"]', 'a[href*="/watch"]',
         'a[href*="/messages"]', 'a[href*="/bookmarks"]'];

    function findBar(el) {
      for (var n = el; n && n !== document.body && n !== document.documentElement;
           n = n.parentElement) {
        var cs = getComputedStyle(n);
        if (cs.position === 'fixed' || cs.position === 'sticky') {
          var r = n.getBoundingClientRect();
          if (r.width >= window.innerWidth * 0.7 && r.height > 0 &&
              r.height <= window.innerHeight * 0.25)
            return n;
        }
      }
      return null;
    }

    var hidden = [];
    var lastScan = 0;

    function hideBars() {
      lastScan = Date.now();
      for (var s = 0; s < selectors.length; s++) {
        var list = document.querySelectorAll(selectors[s]);
        for (var i = 0; i < list.length; i++) {
          var bar = findBar(list[i]);
          if (bar && !bar.getAttribute('data-ub-hidden')) {
            bar.setAttribute('data-ub-hidden', '1');
            bar.style.setProperty('display', 'none', 'important');
            hidden.push(bar);
          }
        }
      }
    }

    // Se vuelve a buscar solo si hace falta (las barras desaparecieron del DOM o
    // pasó un rato): evita trabajo constante mientras haces scroll en el feed.
    var timer = null;
    function schedule() {
      if (timer || document.hidden) return;
      timer = setTimeout(function () {
        timer = null;
        // Se hace cuando el navegador está libre, para no robarle fluidez al scroll
        (window.requestIdleCallback || function (f) { f(); })(function () {
          hidden = hidden.filter(function (b) { return b.isConnected; });
          if (hidden.length === 0 || Date.now() - lastScan > 4000) hideBars();
        }, { timeout: 1500 });
      }, 700);
    }

    hideBars();
    new MutationObserver(schedule).observe(document.documentElement,
      { childList: true, subtree: true });
  } catch (e) {}
})();

// Quita los avisos y botones de "Usar / Abrir / Descargar la app" de Instagram y
// Facebook (banners, botón rosa o azul y ventanas emergentes).
(function () {
  try {
    var host = location.hostname;
    if (!/(^|\.)(instagram|facebook)\.com$/.test(host)) return;

    var storeSel = 'a[href*="play.google.com"],a[href*="apps.apple.com"],' +
                   'a[href*="itunes.apple.com"],a[href*="onelink.me"],a[href*="app_install"]';
    var promo = /^(usar|abrir|descargar|obtener|instalar|get|use|open|install|download)\b.{0,30}\b(app|aplicaci[oó]n|instagram|facebook)\b/i;
    var dismiss = /^(ahora no|no,? gracias|not now|no thanks|cerrar|close|cancelar|cancel)$/i;

    function hide(el) {
      el.style.setProperty('display', 'none', 'important');
    }

    // Capa oscura que cubre toda la pantalla y que rodea a una ventana emergente
    function fullOverlay(n) {
      var top = n;
      for (var p = n.parentElement; p && p !== document.body; p = p.parentElement) {
        var r = p.getBoundingClientRect();
        if (getComputedStyle(p).position === 'fixed' &&
            r.width >= window.innerWidth * 0.9 && r.height >= window.innerHeight * 0.9)
          top = p;
      }
      return top;
    }

    function inFixedOrDialog(el) {
      if (el.closest('[role=dialog],[role=alertdialog]')) return true;
      for (var n = el; n && n !== document.body; n = n.parentElement) {
        var pos = getComputedStyle(n).position;
        if (pos === 'fixed' || pos === 'sticky') return true;
      }
      return false;
    }

    function handle(el) {
      var dlg = el.closest('[role=dialog],[role=alertdialog]');
      if (dlg) {
        // Ventana emergente: primero se intenta cerrarla con su propio "Ahora no"
        var btns = dlg.querySelectorAll('button,[role=button],a');
        for (var i = 0; i < btns.length; i++) {
          var t = (btns[i].innerText || btns[i].getAttribute('aria-label') || '').trim();
          if (dismiss.test(t)) { btns[i].click(); return; }
        }
        hide(fullOverlay(dlg));
        document.documentElement.style.setProperty('overflow', 'auto', 'important');
        document.body.style.setProperty('overflow', 'auto', 'important');
        return;
      }
      // Banner: se oculta el botón junto con su barra (si es pequeña)
      var best = el;
      var n = el.parentElement;
      for (var d = 0; n && d < 4 && n !== document.body; d++, n = n.parentElement) {
        var r = n.getBoundingClientRect();
        if ((n.innerText || '').trim().length > 80 || r.height > window.innerHeight * 0.35) break;
        best = n;
      }
      hide(best);
    }

    function scan() {
      var stores = document.querySelectorAll(storeSel);
      for (var s = 0; s < stores.length; s++) {
        if (stores[s].__ubp) continue;
        stores[s].__ubp = 1;
        if (inFixedOrDialog(stores[s])) handle(stores[s]);
      }
      var els = document.querySelectorAll('a,button,[role=button]');
      for (var i = 0; i < els.length; i++) {
        var el = els[i];
        if (el.__ubp) continue;
        var txt = (el.innerText || '').trim();
        if (!txt) continue;
        el.__ubp = 1;
        if (txt.length <= 40 && promo.test(txt)) handle(el);
      }
    }

    var timer = null;
    function schedule() {
      if (timer || document.hidden) return;
      timer = setTimeout(function () {
        timer = null;
        (window.requestIdleCallback || function (f) { f(); })(scan, { timeout: 1500 });
      }, 1000);
    }

    scan();
    setTimeout(scan, 1500);
    new MutationObserver(schedule).observe(document.documentElement,
      { childList: true, subtree: true });
  } catch (e) {}
})();
)JS");
}

QString InstagramBridge::serviceForUrl(const QUrl &url) const
{
    if (!isAllowedHost(url))
        return QString();
    const QString host = url.host().toLower();
    if (hostMatches(host, QStringLiteral("instagram.com"))
        || hostMatches(host, QStringLiteral("cdninstagram.com")))
        return QStringLiteral("instagram");
    if (host.isEmpty())
        return QString();
    return QStringLiteral("facebook");
}

QString InstagramBridge::startService() const
{
    if (m_launchUrl.isValid() && isAllowedHost(m_launchUrl)) {
        const QString s = serviceForUrl(m_launchUrl);
        if (!s.isEmpty())
            return s;
    }
    QSettings settings;
    const QString last = settings.value(QStringLiteral("lastService")).toString();
    if (last == QLatin1String("facebook"))
        return last;
    return QStringLiteral("instagram");
}

QUrl InstagramBridge::homeUrlFor(const QString &service) const
{
    if (service == QLatin1String("facebook"))
        return QUrl(QString::fromLatin1(kFbHomeUrl));
    return homeUrl();
}

QUrl InstagramBridge::startUrlFor(const QString &service) const
{
    QSettings settings;
    if (service == QLatin1String("facebook")) {
        if (m_launchUrl.isValid() && serviceForUrl(m_launchUrl) == QLatin1String("facebook"))
            return m_launchUrl;
        const QUrl last = settings.value(QStringLiteral("lastUrl_facebook")).toUrl();
        if (last.isValid() && serviceForUrl(last) == QLatin1String("facebook"))
            return last;
        return homeUrlFor(service);
    }

    if (m_launchUrl.isValid() && serviceForUrl(m_launchUrl) == QLatin1String("instagram"))
        return m_launchUrl;
    const QUrl last = settings.value(QStringLiteral("lastUrl")).toUrl();
    if (last.isValid() && isAllowedHost(last))
        return last;
    return homeUrl();
}

void InstagramBridge::rememberUrlFor(const QString &service, const QUrl &url) const
{
    if (service == QLatin1String("facebook")) {
        if (url.scheme().toLower() != QLatin1String("https")
            || serviceForUrl(url) != QLatin1String("facebook"))
            return;
        QSettings settings;
        settings.setValue(QStringLiteral("lastUrl_facebook"), url);
        return;
    }
    rememberUrl(url);
}

void InstagramBridge::rememberService(const QString &service) const
{
    if (service != QLatin1String("instagram") && service != QLatin1String("facebook"))
        return;
    QSettings settings;
    settings.setValue(QStringLiteral("lastService"), service);
}

QVariantList InstagramBridge::menuItems(const QString &service) const
{
    QVariantList list;
    if (service == QLatin1String("facebook")) {
        const QString base = QString::fromLatin1(kFbHomeUrl);
        list << menuItem(QStringLiteral("home"), QStringLiteral("Inicio"),
                         QStringLiteral("go-home"), base)
             << menuItem(QStringLiteral("friends"), QStringLiteral("Amigos"),
                         QStringLiteral("contact-group"), base + QStringLiteral("friends/"))
             << menuItem(QStringLiteral("videos"), QStringLiteral("Videos"),
                         QStringLiteral("media-playback-start"), base + QStringLiteral("watch/"))
             << menuItem(QStringLiteral("messages"), QStringLiteral("Mensajes"),
                         QStringLiteral("message"), base + QStringLiteral("messages/"))
             << menuItem(QStringLiteral("notifications"), QStringLiteral("Notificaciones"),
                         QStringLiteral("notification"), base + QStringLiteral("notifications/"))
             << menuItem(QStringLiteral("create"), QStringLiteral("Crear publicación"),
                         QStringLiteral("add"), base,
                         QString::fromLatin1(kFbCreateScript))
             << menuItem(QStringLiteral("profile"), QStringLiteral("Mi perfil"),
                         QStringLiteral("contact"), base + QStringLiteral("me"));
        return list;
    }

    const QString base = QString::fromLatin1(kHomeUrl);
    list << menuItem(QStringLiteral("home"), QStringLiteral("Inicio"),
                     QStringLiteral("go-home"), base)
         << menuItem(QStringLiteral("explore"), QStringLiteral("Explorar"),
                     QStringLiteral("find"), base + QStringLiteral("explore/"))
         << menuItem(QStringLiteral("reels"), QStringLiteral("Reels"),
                     QStringLiteral("media-playback-start"), base + QStringLiteral("reels/"))
         << menuItem(QStringLiteral("messages"), QStringLiteral("Mensajes"),
                     QStringLiteral("message"), base + QStringLiteral("direct/inbox/"))
         << menuItem(QStringLiteral("notifications"), QStringLiteral("Notificaciones"),
                     QStringLiteral("notification"), base + QStringLiteral("accounts/activity/"))
         << menuItem(QStringLiteral("create"), QStringLiteral("Crear publicación"),
                     QStringLiteral("add"), base,
                     QString::fromLatin1(kIgCreateScript));
    return list;
}
