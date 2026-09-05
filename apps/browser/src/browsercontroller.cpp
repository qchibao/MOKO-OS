#include "browsercontroller.h"

#include "browserurl.h"
#include "livemarker.h"

#include <QVariant>

#include <unistd.h>

#include <utility>

BrowserController::BrowserController(QString initialInput, bool smokeTest,
                                     QUrl validationDownloadUrl, QObject *parent)
    : QObject(parent)
    , m_history(this)
    , m_downloads(this)
    , m_initialUrl(smokeTest ? QUrl(QStringLiteral("about:blank"))
                             : normalizedBrowserUrl(initialInput))
    , m_validationDownloadUrl(std::move(validationDownloadUrl))
    , m_smokeTest(smokeTest)
    , m_statusMessage(QStringLiteral("Ready"))
{
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

QUrl BrowserController::urlFromInput(const QString &input) const
{
    return normalizedBrowserUrl(input);
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
