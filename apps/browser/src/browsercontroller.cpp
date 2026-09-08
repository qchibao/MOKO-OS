#include "browsercontroller.h"

#include "browserurl.h"
#include "livemarker.h"

#include <QVariant>
#include <QFileInfo>
#include <QSettings>
#include <QUrlQuery>

#include <unistd.h>

#include <utility>

BrowserController::BrowserController(QString initialInput, bool smokeTest,
                                     QUrl validationDownloadUrl, QObject *parent)
    : QObject(parent)
    , m_history(this)
    , m_downloads(this)
    , m_initialUrl(smokeTest ? QUrl(QStringLiteral("about:blank"))
                             : initialInput.trimmed().isEmpty()
                                 ? QUrl(QStringLiteral("moko://home"))
                                 : normalizedBrowserUrl(initialInput))
    , m_validationDownloadUrl(std::move(validationDownloadUrl))
    , m_smokeTest(smokeTest)
    , m_statusMessage(QStringLiteral("Ready"))
{
    loadQuickSites();
    connect(&m_downloads, &BrowserDownloadModel::statusMessage,
            this, &BrowserController::setStatusMessage);
    connect(&m_downloads, &BrowserDownloadModel::downloadCompleted, this,
            [this](const QString &, qint64) { setStatusMessage(QStringLiteral("Download complete")); });
}

BrowserHistoryModel *BrowserController::historyModel()
{
    return &m_history;
}

BrowserDownloadModel *BrowserController::downloadModel()
{
    return &m_downloads;
}

QUrl BrowserController::initialUrl() const
{
    return m_initialUrl;
}

QUrl BrowserController::validationDownloadUrl() const
{
    return m_validationDownloadUrl;
}

bool BrowserController::smokeTest() const
{
    return m_smokeTest;
}

QString BrowserController::statusMessage() const
{
    return m_statusMessage;
}

QVariantList BrowserController::quickSites() const
{
    return m_quickSites;
}

QUrl BrowserController::urlFromInput(const QString &input) const
{
    return normalizedBrowserUrl(input);
}

QUrl BrowserController::localFileUrl(const QString &path) const
{
    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.isReadable()
        ? QUrl::fromLocalFile(info.absoluteFilePath()) : QUrl();
}

void BrowserController::recordLoad(const QString &title, const QUrl &url,
                                   bool succeeded, const QString &error)
{
    const QString scheme = url.scheme().toLower();
    const QString host = markerValue(url.host());
    if (!succeeded) {
        setStatusMessage(error.trimmed().isEmpty() ? QStringLiteral("Page could not be loaded")
                                                   : error.trimmed());
        writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_PAGE state=failed scheme=%1 host=%2 uid=%3")
                               .arg(markerValue(scheme), host)
                               .arg(geteuid()));
        return;
    }
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        m_history.addVisit(title, url);
    setStatusMessage(QStringLiteral("Ready"));
    writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_PAGE state=loaded scheme=%1 host=%2 uid=%3")
                           .arg(markerValue(scheme), host)
                           .arg(geteuid()));
}

void BrowserController::reportJavaScript(const QUrl &url, const QVariant &result)
{
    const bool passed = result.toString() == QStringLiteral("moko-js-ready");
    writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_JAVASCRIPT state=%1 scheme=%2 host=%3 uid=%4")
                           .arg(passed ? QStringLiteral("pass") : QStringLiteral("fail"),
                                markerValue(url.scheme().toLower()), markerValue(url.host()))
                           .arg(geteuid()));
    if (!passed)
        setStatusMessage(QStringLiteral("Page script validation failed"));
    emit javaScriptValidated(url, passed);
}

bool BrowserController::showDownloadsInFiles()
{
    return m_downloads.showInFiles();
}

bool BrowserController::browserHistorySwipeEnabled() const
{
    return QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"))
        .value(QStringLiteral("input/browserHistorySwipe"), true).toBool();
}

void BrowserController::loadQuickSites()
{
    const QSettings settings(QStringLiteral("MOKO"), QStringLiteral("MOKO Browser"));
    const QStringList stored = settings.value(QStringLiteral("quickSites")).toStringList();
    const auto append = [this](const QString &label, const QString &url) {
        m_quickSites.append(QVariantMap{{QStringLiteral("label"), label},
                                        {QStringLiteral("url"), url}});
    };
    for (const QString &entry : stored) {
        const int separator = entry.indexOf(u'\t');
        if (separator <= 0)
            continue;
        const QString label = entry.left(separator).trimmed();
        const QString url = entry.mid(separator + 1).trimmed();
        const QUrl parsed(url);
        if (!label.isEmpty() && parsed.isValid()
            && (parsed.scheme() == QStringLiteral("http")
                || parsed.scheme() == QStringLiteral("https")))
            append(label.left(80), url.left(512));
    }
    if (m_quickSites.isEmpty()) {
        append(QStringLiteral("MOKO OS"), QStringLiteral("https://moko.asia/"));
        append(QStringLiteral("Debian"), QStringLiteral("https://www.debian.org/"));
        append(QStringLiteral("Documentation"), QStringLiteral("https://docs.moko.asia/"));
        QStringList encoded;
        for (const QVariant &site : m_quickSites) {
            const QVariantMap map = site.toMap();
            encoded.append(map.value(QStringLiteral("label")).toString() + u'\t'
                           + map.value(QStringLiteral("url")).toString());
        }
        QSettings writable(QStringLiteral("MOKO"), QStringLiteral("MOKO Browser"));
        writable.setValue(QStringLiteral("quickSites"), encoded);
    }
}

bool BrowserController::setQuickSite(int index, const QString &label, const QString &url)
{
    const QString cleanLabel = label.trimmed().left(80);
    const QUrl parsed(url.trimmed());
    if (cleanLabel.isEmpty() || !parsed.isValid()
        || (parsed.scheme() != QStringLiteral("http")
            && parsed.scheme() != QStringLiteral("https"))
        || parsed.host().isEmpty())
        return false;
    const QVariantMap value{{QStringLiteral("label"), cleanLabel},
                            {QStringLiteral("url"), parsed.toString().left(512)}};
    if (index < 0 || index >= m_quickSites.size())
        m_quickSites.append(value);
    else
        m_quickSites[index] = value;
    QStringList encoded;
    for (const QVariant &site : m_quickSites) {
        const QVariantMap map = site.toMap();
        encoded.append(map.value(QStringLiteral("label")).toString() + u'\t'
                       + map.value(QStringLiteral("url")).toString());
    }
    QSettings settings(QStringLiteral("MOKO"), QStringLiteral("MOKO Browser"));
    settings.setValue(QStringLiteral("quickSites"), encoded);
    emit quickSitesChanged();
    return true;
}

bool BrowserController::addQuickSite(const QString &label, const QString &url)
{
    return setQuickSite(-1, label, url);
}

bool BrowserController::updateQuickSite(int index, const QString &label, const QString &url)
{
    return setQuickSite(index, label, url);
}

bool BrowserController::removeQuickSite(int index)
{
    if (index < 0 || index >= m_quickSites.size())
        return false;
    m_quickSites.removeAt(index);
    QStringList encoded;
    for (const QVariant &site : m_quickSites) {
        const QVariantMap map = site.toMap();
        encoded.append(map.value(QStringLiteral("label")).toString() + u'\t'
                       + map.value(QStringLiteral("url")).toString());
    }
    QSettings settings(QStringLiteral("MOKO"), QStringLiteral("MOKO Browser"));
    settings.setValue(QStringLiteral("quickSites"), encoded);
    emit quickSitesChanged();
    return true;
}

bool BrowserController::moveQuickSite(int from, int to)
{
    if (from < 0 || from >= m_quickSites.size() || to < 0 || to >= m_quickSites.size())
        return false;
    if (from == to)
        return true;
    m_quickSites.move(from, to);
    QStringList encoded;
    for (const QVariant &site : std::as_const(m_quickSites)) {
        const QVariantMap map = site.toMap();
        encoded.append(map.value(QStringLiteral("label")).toString() + u'\t'
                       + map.value(QStringLiteral("url")).toString());
    }
    QSettings settings(QStringLiteral("MOKO"), QStringLiteral("MOKO Browser"));
    settings.setValue(QStringLiteral("quickSites"), encoded);
    emit quickSitesChanged();
    return true;
}

void BrowserController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

QString BrowserController::markerValue(QString value)
{
    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_') {
            character = u'_';
        }
    }
    return value.left(100);
}
