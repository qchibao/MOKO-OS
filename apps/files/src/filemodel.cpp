#include "filemodel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QLocale>
#include <QUrl>
#include <QUuid>

#include <algorithm>

FileModel::FileModel(QObject *parent, const QString &initialPath)
    : QAbstractListModel(parent)
{
    const QString start = initialPath.isEmpty() ? QDir::homePath() : initialPath;
    navigateInternal(start, true);
}

int FileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !validRow(index.row()))
        return {};

    const Entry &item = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return item.name;
    case PathRole:
        return item.path;
    case UrlRole:
        return item.url;
    case DirectoryRole:
        return item.directory;
    case SymbolicLinkRole:
        return item.symbolicLink;
    case SizeRole:
        return item.size;
    case SizeTextRole:
        return item.directory ? QStringLiteral("Folder") : formatSize(item.size);
    case ModifiedRole:
        return item.modified;
    case ModifiedTextRole:
        return item.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case TypeRole:
        return item.type;
    case WritableRole:
        return item.writable;
    default:
        return {};
    }
}

QHash<int, QByteArray> FileModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {UrlRole, "url"},
        {DirectoryRole, "isDirectory"},
        {SymbolicLinkRole, "isSymbolicLink"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {ModifiedRole, "modified"},
        {ModifiedTextRole, "modifiedText"},
        {TypeRole, "typeName"},
        {WritableRole, "isWritable"},
    };
}

QString FileModel::currentPath() const
{
    return m_currentPath;
}

QString FileModel::displayPath() const
{
    const QString home = QDir::cleanPath(QDir::homePath());
    if (m_currentPath == home)
        return QStringLiteral("Home");
    if (m_currentPath.startsWith(home + u'/'))
        return QStringLiteral("Home") + m_currentPath.mid(home.size());
    return QDir::toNativeSeparators(m_currentPath);
}

bool FileModel::canGoBack() const
{
    return m_historyIndex > 0;
}

bool FileModel::canGoForward() const
{
    return m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size();
}

bool FileModel::canPaste() const
{
    return !m_clipboardSource.isEmpty();
}

QString FileModel::clipboardMode() const
{
    return m_clipboardMode;
}

QString FileModel::statusMessage() const
{
    return m_statusMessage;
}

bool FileModel::navigateHome()
{
    return navigateInternal(QDir::homePath(), true);
}

bool FileModel::navigateTo(const QString &path)
{
    return navigateInternal(path, true);
}

bool FileModel::navigateUp()
{
    QDir directory(m_currentPath);
    if (!directory.cdUp())
        return false;
    return navigateInternal(directory.absolutePath(), true);
}

bool FileModel::goBack()
{
    if (!canGoBack())
        return false;
    --m_historyIndex;
    if (!navigateInternal(m_history.at(m_historyIndex), false)) {
        ++m_historyIndex;
        return false;
    }
    emit historyChanged();
    return true;
}

bool FileModel::goForward()
{
    if (!canGoForward())
        return false;
    ++m_historyIndex;
    if (!navigateInternal(m_history.at(m_historyIndex), false)) {
        --m_historyIndex;
        return false;
    }
    emit historyChanged();
    return true;
}

bool FileModel::refresh()
{
    return reloadEntries();
}

bool FileModel::openEntry(int row)
{
    if (!validRow(row))
        return fail(QStringLiteral("Select a file or folder first."));

    const Entry item = m_entries.at(row);
    if (item.directory)
        return navigateInternal(item.path, true);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(item.path)))
        return fail(QStringLiteral("No system application could open %1.").arg(item.name));
    setStatusMessage(QStringLiteral("Opened %1").arg(item.name));
    return true;
}

QVariantMap FileModel::entry(int row) const
{
    if (!validRow(row))
        return {};
    const Entry &item = m_entries.at(row);
    return {
        {QStringLiteral("name"), item.name},
        {QStringLiteral("path"), item.path},
        {QStringLiteral("isDirectory"), item.directory},
        {QStringLiteral("typeName"), item.type},
        {QStringLiteral("sizeText"), item.directory ? QStringLiteral("Folder") : formatSize(item.size)},
        {QStringLiteral("modifiedText"), item.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))},
        {QStringLiteral("isWritable"), item.writable},
    };
}

QVariantList FileModel::propertiesFor(int row) const
{
    if (!validRow(row))
        return {};
    const Entry &item = m_entries.at(row);
    const QFileInfo info(item.path);
    const QString permissions = QStringLiteral("%1%2%3")
                                    .arg(info.isReadable() ? QStringLiteral("Read ") : QString(),
                                         info.isWritable() ? QStringLiteral("Write ") : QString(),
                                         info.isExecutable() ? QStringLiteral("Execute") : QString())
                                    .trimmed();
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Name")},
                    {QStringLiteral("value"), item.name}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Location")},
                    {QStringLiteral("value"), QDir::toNativeSeparators(info.absolutePath())}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Type")},
                    {QStringLiteral("value"), item.type}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Size")},
                    {QStringLiteral("value"), item.directory ? QStringLiteral("Folder") : formatSize(item.size)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Modified")},
                    {QStringLiteral("value"), QLocale().toString(item.modified, QLocale::LongFormat)}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Permissions")},
                    {QStringLiteral("value"), permissions.isEmpty() ? QStringLiteral("None") : permissions}},
    };
}

bool FileModel::createFolder(const QString &name)
{
    QString cleanName;
    if (!validateName(name, &cleanName))
        return false;
    const QString path = QDir(m_currentPath).filePath(cleanName);
    if (QFileInfo::exists(path))
        return fail(QStringLiteral("An item named %1 already exists.").arg(cleanName));
    if (!QDir(m_currentPath).mkdir(cleanName))
        return fail(QStringLiteral("Could not create %1.").arg(cleanName));
    setStatusMessage(QStringLiteral("Created folder %1").arg(cleanName));
    return reloadEntries();
}

bool FileModel::renameEntry(int row, const QString &newName)
{
    if (!validRow(row))
        return fail(QStringLiteral("Select an item to rename."));
    QString cleanName;
    if (!validateName(newName, &cleanName))
        return false;

    const Entry item = m_entries.at(row);
    const QString destination = QDir(QFileInfo(item.path).absolutePath()).filePath(cleanName);
    if (item.path == destination)
        return true;
    if (QFileInfo::exists(destination))
        return fail(QStringLiteral("An item named %1 already exists.").arg(cleanName));
    if (!QFile::rename(item.path, destination))
        return fail(QStringLiteral("Could not rename %1.").arg(item.name));
    setStatusMessage(QStringLiteral("Renamed %1 to %2").arg(item.name, cleanName));
    return reloadEntries();
}

bool FileModel::stageCopy(int row)
{
    if (!validRow(row))
        return fail(QStringLiteral("Select an item to copy."));
    m_clipboardSource = m_entries.at(row).path;
    m_clipboardMode = QStringLiteral("copy");
    setStatusMessage(QStringLiteral("Ready to copy %1").arg(m_entries.at(row).name));
    emit clipboardChanged();
    return true;
}

bool FileModel::stageMove(int row)
{
    if (!validRow(row))
        return fail(QStringLiteral("Select an item to move."));
    m_clipboardSource = m_entries.at(row).path;
    m_clipboardMode = QStringLiteral("move");
    setStatusMessage(QStringLiteral("Ready to move %1").arg(m_entries.at(row).name));
    emit clipboardChanged();
    return true;
}

bool FileModel::paste()
{
    if (!canPaste())
        return fail(QStringLiteral("Nothing is ready to paste."));
    const QFileInfo sourceInfo(m_clipboardSource);
    if (!sourceInfo.exists() && !sourceInfo.isSymLink()) {
        m_clipboardSource.clear();
        m_clipboardMode.clear();
        emit clipboardChanged();
        return fail(QStringLiteral("The source item no longer exists."));
    }

    const QString destination = QDir(m_currentPath).filePath(sourceInfo.fileName());
    const QString sourcePath = QDir::cleanPath(sourceInfo.absoluteFilePath());
    const QString destinationPath = QDir::cleanPath(QFileInfo(destination).absoluteFilePath());
    if (sourceInfo.isDir()
        && destinationPath.startsWith(sourcePath + u'/')) {
        return fail(QStringLiteral("A folder cannot be copied or moved inside itself."));
    }
    if (destinationPath == sourcePath)
        return fail(QStringLiteral("Choose a different destination folder."));
    if (QFileInfo::exists(destination))
        return fail(QStringLiteral("An item named %1 already exists here.").arg(sourceInfo.fileName()));

    QString error;
    bool succeeded = false;
    if (m_clipboardMode == QStringLiteral("move")) {
        succeeded = QFile::rename(m_clipboardSource, destination);
        if (!succeeded)
            error = QStringLiteral("Move failed. Cross-device moves are not performed automatically.");
    } else {
        succeeded = copyPath(m_clipboardSource, destination, &error);
    }
    if (!succeeded)
        return fail(error.isEmpty() ? QStringLiteral("The operation failed.") : error);

    setStatusMessage(QStringLiteral("%1 %2")
                         .arg(m_clipboardMode == QStringLiteral("move") ? QStringLiteral("Moved")
                                                                         : QStringLiteral("Copied"),
                              sourceInfo.fileName()));
    if (m_clipboardMode == QStringLiteral("move")) {
        m_clipboardSource.clear();
        m_clipboardMode.clear();
        emit clipboardChanged();
    }
    return reloadEntries();
}

QString FileModel::requestDelete(int row)
{
    if (!validRow(row)) {
        fail(QStringLiteral("Select an item to delete."));
        return {};
    }
    m_pendingDeletePath = m_entries.at(row).path;
    m_pendingDeleteToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit deleteConfirmationRequested(m_entries.at(row).name,
                                     m_pendingDeletePath,
                                     m_pendingDeleteToken);
    return m_pendingDeleteToken;
}

bool FileModel::confirmDelete(const QString &token)
{
    if (token.isEmpty() || token != m_pendingDeleteToken || m_pendingDeletePath.isEmpty())
        return fail(QStringLiteral("Delete confirmation expired."));

    const QString path = m_pendingDeletePath;
    const QString name = QFileInfo(path).fileName();
    cancelDelete();
    QString error;
    if (!removePath(path, &error))
        return fail(error);
    setStatusMessage(QStringLiteral("Deleted %1").arg(name));
    return reloadEntries();
}

void FileModel::cancelDelete()
{
    m_pendingDeletePath.clear();
    m_pendingDeleteToken.clear();
}

bool FileModel::navigateInternal(const QString &path, bool addHistory)
{
    QString candidate = QDir::fromNativeSeparators(path.trimmed());
    if (candidate.isEmpty())
        return fail(QStringLiteral("Enter a folder path."));
    if (QDir::isRelativePath(candidate))
        candidate = QDir(m_currentPath.isEmpty() ? QDir::homePath() : m_currentPath).filePath(candidate);

    QFileInfo info(QDir::cleanPath(candidate));
    if (!info.exists() || !info.isDir())
        return fail(QStringLiteral("Folder not found: %1").arg(QDir::toNativeSeparators(candidate)));
    if (!info.isReadable())
        return fail(QStringLiteral("This folder is not readable."));

    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                                                 : info.canonicalFilePath();
    const bool changed = canonical != m_currentPath;
    m_currentPath = canonical;
    if (!reloadEntries())
        return false;

    if (addHistory && (m_historyIndex < 0 || m_history.value(m_historyIndex) != canonical)) {
        while (m_history.size() > m_historyIndex + 1)
            m_history.removeLast();
        m_history.append(canonical);
        m_historyIndex = m_history.size() - 1;
        emit historyChanged();
    }
    if (changed)
        emit currentPathChanged();
    setStatusMessage(QStringLiteral("%1 item%2")
                         .arg(m_entries.size())
                         .arg(m_entries.size() == 1 ? QString() : QStringLiteral("s")));
    return true;
}

bool FileModel::reloadEntries()
{
    QDir directory(m_currentPath);
    if (!directory.isReadable())
        return fail(QStringLiteral("Could not read this folder."));

    QMimeDatabase mimeDatabase;
    QVector<Entry> entries;
    const QFileInfoList infos = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                        QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        Entry item;
        item.name = info.fileName();
        item.path = info.absoluteFilePath();
        item.url = QUrl::fromLocalFile(item.path).toString();
        item.directory = info.isDir() && !info.isSymLink();
        item.symbolicLink = info.isSymLink();
        item.size = info.size();
        item.modified = info.lastModified();
        item.writable = info.isWritable();
        if (item.directory) {
            item.type = QStringLiteral("Folder");
        } else if (item.symbolicLink) {
            item.type = QStringLiteral("Symbolic link");
        } else {
            item.type = mimeDatabase.mimeTypeForFile(info).comment();
            if (item.type.isEmpty())
                item.type = QStringLiteral("File");
        }
        entries.append(std::move(item));
    }

    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit countChanged();
    return true;
}

bool FileModel::validRow(int row) const
{
    return row >= 0 && row < m_entries.size();
}

bool FileModel::validateName(const QString &name, QString *cleanName)
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty() || candidate == QStringLiteral(".") || candidate == QStringLiteral("..")
        || candidate.contains(u'/') || candidate.contains(u'\\')) {
        return fail(QStringLiteral("Use a simple name without path separators."));
    }
    if (cleanName)
        *cleanName = candidate;
    return true;
}

bool FileModel::copyPath(const QString &source, const QString &destination, QString *error) const
{
    const QFileInfo sourceInfo(source);
    if (sourceInfo.isSymLink()) {
        if (QFile::link(sourceInfo.symLinkTarget(), destination))
            return true;
        *error = QStringLiteral("Could not copy symbolic link %1.").arg(sourceInfo.fileName());
        return false;
    }
    if (!sourceInfo.isDir()) {
        if (QFile::copy(source, destination))
            return true;
        *error = QStringLiteral("Could not copy %1.").arg(sourceInfo.fileName());
        return false;
    }

    if (!QDir().mkpath(destination)) {
        *error = QStringLiteral("Could not create destination folder.");
        return false;
    }
    const QFileInfoList children = QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &child : children) {
        if (!copyPath(child.absoluteFilePath(), QDir(destination).filePath(child.fileName()), error)) {
            QDir(destination).removeRecursively();
            return false;
        }
    }
    return true;
}

bool FileModel::removePath(const QString &path, QString *error) const
{
    const QFileInfo info(path);
    bool removed = false;
    if (info.isDir() && !info.isSymLink())
        removed = QDir(path).removeRecursively();
    else
        removed = QFile::remove(path);
    if (!removed)
        *error = QStringLiteral("Could not delete %1.").arg(info.fileName());
    return removed;
}

void FileModel::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

bool FileModel::fail(const QString &message)
{
    setStatusMessage(message);
    emit errorOccurred(message);
    return false;
}

QString FileModel::formatSize(qint64 bytes)
{
    static const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"),
                                      QStringLiteral("MB"), QStringLiteral("GB"),
                                      QStringLiteral("TB")};
    double value = std::max<qint64>(0, bytes);
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? QStringLiteral("%1 %2").arg(bytes).arg(units.at(unit))
                     : QStringLiteral("%1 %2").arg(value, 0, 'f', value < 10 ? 1 : 0).arg(units.at(unit));
}
