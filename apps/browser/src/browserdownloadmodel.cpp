#include "browserdownloadmodel.h"

#include "livemarker.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWebEngineDownloadRequest>

#include <unistd.h>

#include <algorithm>
#include <utility>

BrowserDownloadModel::BrowserDownloadModel(QObject *parent, QString downloadDirectory)
    : QAbstractListModel(parent)
    , m_downloadDirectory(std::move(downloadDirectory))
{
    if (m_downloadDirectory.isEmpty())
        m_downloadDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (m_downloadDirectory.isEmpty())
        m_downloadDirectory = QDir(QDir::homePath()).filePath(QStringLiteral("Downloads"));
    m_downloadDirectory = QDir::cleanPath(QFileInfo(m_downloadDirectory).absoluteFilePath());
    QDir().mkpath(m_downloadDirectory);
}

int BrowserDownloadModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant BrowserDownloadModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case FileNameRole:
        return entry.fileName;
    case FilePathRole:
        return entry.filePath;
    case SourceUrlRole:
        return entry.sourceUrl;
    case MimeTypeRole:
        return entry.mimeType;
    case StateRole:
        return entry.state;
    case StatusTextRole:
        return entry.statusText;
    case ReceivedBytesRole:
        return entry.receivedBytes;
    case TotalBytesRole:
        return entry.totalBytes;
    case ProgressRole:
        return entry.progress;
    case CanCancelRole:
        return entry.canCancel;
    case CanOpenRole:
        return entry.canOpen;
    default:
        return {};
    }
}

QHash<int, QByteArray> BrowserDownloadModel::roleNames() const
{
    return {{FileNameRole, "fileName"},
            {FilePathRole, "filePath"},
            {SourceUrlRole, "sourceUrl"},
            {MimeTypeRole, "mimeType"},
            {StateRole, "state"},
            {StatusTextRole, "statusText"},
            {ReceivedBytesRole, "receivedBytes"},
            {TotalBytesRole, "totalBytes"},
            {ProgressRole, "progress"},
            {CanCancelRole, "canCancel"},
            {CanOpenRole, "canOpen"}};
}

QString BrowserDownloadModel::downloadDirectory() const
{
    return m_downloadDirectory;
}

bool BrowserDownloadModel::acceptDownload(QObject *downloadObject)
{
    auto *request = qobject_cast<QWebEngineDownloadRequest *>(downloadObject);
    if (!request || request->state() != QWebEngineDownloadRequest::DownloadRequested)
        return fail(QStringLiteral("The download request is no longer available."));

    const QString fileName = uniqueFileName(request->suggestedFileName());
    request->setDownloadDirectory(m_downloadDirectory);
    request->setDownloadFileName(fileName);

    Entry entry;
    entry.request = request;
    entry.fileName = fileName;
    entry.filePath = QDir(m_downloadDirectory).filePath(fileName);
    entry.sourceUrl = request->url();
    entry.mimeType = request->mimeType();
    entry.state = QStringLiteral("starting");
    entry.statusText = QStringLiteral("Starting download");
    entry.canCancel = true;

    beginInsertRows({}, 0, 0);
    m_entries.prepend(std::move(entry));
    endInsertRows();
    emit countChanged();

    const auto refresh = [this, request]() { updateRequest(request); };
    connect(request, &QWebEngineDownloadRequest::stateChanged, this, refresh);
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this, refresh);
    connect(request, &QWebEngineDownloadRequest::totalBytesChanged, this, refresh);
    connect(request, &QWebEngineDownloadRequest::interruptReasonChanged, this, refresh);
    connect(request, &QObject::destroyed, this, [this, request]() {
        const int row = rowForRequest(request);
        if (row >= 0)
            m_entries[row].request.clear();
    });

    request->accept();
    updateRequest(request);
    writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_DOWNLOAD state=started file=%1 uid=%2")
                           .arg(fileName.left(120))
                           .arg(geteuid()));
    return true;
}

bool BrowserDownloadModel::cancel(int row)
{
    if (row < 0 || row >= m_entries.size() || !m_entries.at(row).request
        || !m_entries.at(row).canCancel) {
        return fail(QStringLiteral("This download cannot be cancelled."));
    }
    m_entries.at(row).request->cancel();
    return true;
}

bool BrowserDownloadModel::open(int row)
{
    if (row < 0 || row >= m_entries.size() || !m_entries.at(row).canOpen)
        return fail(QStringLiteral("The downloaded file is not ready yet."));
    const QString path = m_entries.at(row).filePath;
    if (!isPathInside(path, m_downloadDirectory) || !QFileInfo::exists(path))
        return fail(QStringLiteral("The downloaded file is no longer available."));
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        return fail(QStringLiteral("No installed application can open this file."));
    emit statusMessage(QStringLiteral("Opened %1").arg(m_entries.at(row).fileName));
    return true;
}

bool BrowserDownloadModel::showInFiles(int row)
{
    QString directory = m_downloadDirectory;
    if (row >= 0 && row < m_entries.size())
        directory = QFileInfo(m_entries.at(row).filePath).absolutePath();
    if (!isPathInside(directory, m_downloadDirectory))
        return fail(QStringLiteral("The download location is outside Downloads."));

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("moko-files"));
    if (executable.isEmpty())
        return fail(QStringLiteral("MOKO Files is not installed."));
    const bool launched = QProcess::startDetached(
        executable, {QUrl::fromLocalFile(directory).toString(QUrl::FullyEncoded)});
    if (!launched)
        return fail(QStringLiteral("MOKO Files could not be opened."));
    writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_ACTION action=show_downloads state=accepted uid=%1")
                           .arg(geteuid()));
    emit statusMessage(QStringLiteral("Opened Downloads in MOKO Files"));
    return true;
}

QVariantMap BrowserDownloadModel::entry(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(row);
    return {{QStringLiteral("fileName"), entry.fileName},
            {QStringLiteral("filePath"), entry.filePath},
            {QStringLiteral("sourceUrl"), entry.sourceUrl},
            {QStringLiteral("mimeType"), entry.mimeType},
            {QStringLiteral("state"), entry.state},
            {QStringLiteral("statusText"), entry.statusText},
            {QStringLiteral("progress"), entry.progress},
            {QStringLiteral("canCancel"), entry.canCancel},
            {QStringLiteral("canOpen"), entry.canOpen}};
}

void BrowserDownloadModel::clearFinished()
{
    for (int row = m_entries.size() - 1; row >= 0; --row) {
        if (m_entries.at(row).canCancel)
            continue;
        beginRemoveRows({}, row, row);
        m_entries.removeAt(row);
        endRemoveRows();
    }
    emit countChanged();
}

QString BrowserDownloadModel::safeFileName(const QString &suggestedFileName)
{
    QString name = QFileInfo(suggestedFileName.trimmed()).fileName();
    name.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")), QStringLiteral("_"));
    name.replace(u'/', u'_');
    name.replace(u'\\', u'_');
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        name = QStringLiteral("download");
    return name.left(180);
}

bool BrowserDownloadModel::isPathInside(const QString &path, const QString &directory)
{
    const QString root = QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    const QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return candidate == root || candidate.startsWith(root + u'/');
}

int BrowserDownloadModel::rowForRequest(const QWebEngineDownloadRequest *request) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).request == request)
            return row;
    }
    return -1;
}

QString BrowserDownloadModel::uniqueFileName(const QString &suggestedFileName) const
{
    const QString safeName = safeFileName(suggestedFileName);
    QFileInfo info(safeName);
    const QString baseName = info.completeBaseName().isEmpty() ? QStringLiteral("download")
                                                               : info.completeBaseName();
    const QString suffix = info.completeSuffix();
    QString candidate = safeName;
    int number = 2;
    const auto isTaken = [this](const QString &name) {
        if (QFileInfo::exists(QDir(m_downloadDirectory).filePath(name)))
            return true;
        return std::any_of(m_entries.cbegin(), m_entries.cend(), [&name](const Entry &entry) {
            return entry.fileName == name;
        });
    };
    while (isTaken(candidate)) {
        candidate = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(number++)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(number++).arg(suffix);
    }
    return candidate;
}

void BrowserDownloadModel::updateRequest(QWebEngineDownloadRequest *request)
{
    const int row = rowForRequest(request);
    if (row < 0)
        return;
    Entry &entry = m_entries[row];
    entry.receivedBytes = request->receivedBytes();
    entry.totalBytes = request->totalBytes();
    entry.progress = entry.totalBytes > 0
        ? std::clamp(static_cast<double>(entry.receivedBytes) / entry.totalBytes, 0.0, 1.0)
        : 0.0;

    switch (request->state()) {
    case QWebEngineDownloadRequest::DownloadRequested:
        entry.state = QStringLiteral("starting");
        entry.statusText = QStringLiteral("Starting download");
        entry.canCancel = true;
        break;
    case QWebEngineDownloadRequest::DownloadInProgress:
        entry.state = QStringLiteral("downloading");
        entry.statusText = entry.totalBytes > 0
            ? QStringLiteral("%1 of %2").arg(formatBytes(entry.receivedBytes),
                                              formatBytes(entry.totalBytes))
            : QStringLiteral("%1 downloaded").arg(formatBytes(entry.receivedBytes));
        entry.canCancel = true;
        break;
    case QWebEngineDownloadRequest::DownloadCompleted:
        entry.state = QStringLiteral("complete");
        entry.statusText = QStringLiteral("Complete - %1").arg(formatBytes(entry.receivedBytes));
        entry.canCancel = false;
        entry.canOpen = QFileInfo::exists(entry.filePath)
            && isPathInside(entry.filePath, m_downloadDirectory);
        if (!entry.completionReported) {
            entry.completionReported = true;
            writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_DOWNLOAD state=completed file=%1 bytes=%2 uid=%3")
                                   .arg(entry.fileName.left(120))
                                   .arg(entry.receivedBytes)
                                   .arg(geteuid()));
            emit downloadCompleted(entry.filePath, entry.receivedBytes);
        }
        break;
    case QWebEngineDownloadRequest::DownloadCancelled:
        entry.state = QStringLiteral("cancelled");
        entry.statusText = QStringLiteral("Cancelled");
        entry.canCancel = false;
        break;
    case QWebEngineDownloadRequest::DownloadInterrupted:
        entry.state = QStringLiteral("failed");
        entry.statusText = request->interruptReasonString().isEmpty()
            ? QStringLiteral("Download failed") : request->interruptReasonString();
        entry.canCancel = false;
        writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_DOWNLOAD state=failed uid=%1")
                               .arg(geteuid()));
        break;
    }
    emit dataChanged(index(row), index(row));
}

bool BrowserDownloadModel::fail(const QString &message)
{
    emit statusMessage(message);
    return false;
}

QString BrowserDownloadModel::formatBytes(qint64 bytes)
{
    static const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"),
                                      QStringLiteral("MB"), QStringLiteral("GB")};
    double value = std::max<qint64>(0, bytes);
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? QStringLiteral("%1 %2").arg(static_cast<qint64>(value)).arg(units.at(unit))
                     : QStringLiteral("%1 %2").arg(value, 0, 'f', value < 10 ? 1 : 0).arg(units.at(unit));
}
