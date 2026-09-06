#include "mokofilepickermodel.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

#include <utility>

MokoFilePickerModel::MokoFilePickerModel(QObject *parent)
    : QAbstractListModel(parent)
{
    navigateHome();
}

int MokoFilePickerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant MokoFilePickerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !validRow(index.row()))
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case DirectoryRole:
        return entry.directory;
    case SizeTextRole:
        return entry.sizeText;
    case ModifiedTextRole:
        return entry.modifiedText;
    default:
        return {};
    }
}

QHash<int, QByteArray> MokoFilePickerModel::roleNames() const
{
    return {{NameRole, "name"},
            {PathRole, "path"},
            {DirectoryRole, "isDirectory"},
            {SizeTextRole, "sizeText"},
            {ModifiedTextRole, "modifiedText"}};
}

QString MokoFilePickerModel::currentPath() const { return m_currentPath; }

QString MokoFilePickerModel::displayPath() const
{
    const QString home = QDir::cleanPath(QDir::homePath());
    if (m_currentPath == home)
        return QStringLiteral("Home");
    if (m_currentPath.startsWith(home + u'/'))
        return QStringLiteral("Home") + m_currentPath.mid(home.size());
    return QDir::toNativeSeparators(m_currentPath);
}

QString MokoFilePickerModel::statusMessage() const { return m_statusMessage; }

bool MokoFilePickerModel::navigateHome()
{
    return navigateTo(QDir::homePath());
}

bool MokoFilePickerModel::navigateUp()
{
    QDir directory(m_currentPath);
    return directory.cdUp() && navigateTo(directory.absolutePath());
}

bool MokoFilePickerModel::navigateTo(const QString &path)
{
    QString candidate = QDir::fromNativeSeparators(path.trimmed());
    if (candidate.isEmpty())
        return fail(QStringLiteral("Choose a folder."));
    if (QDir::isRelativePath(candidate))
        candidate = QDir(m_currentPath.isEmpty() ? QDir::homePath() : m_currentPath)
                        .filePath(candidate);
    const QFileInfo info(QDir::cleanPath(candidate));
    if (!info.exists() || !info.isDir() || !info.isReadable())
        return fail(QStringLiteral("That folder is not readable."));
    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                                                 : info.canonicalFilePath();
    const bool changed = canonical != m_currentPath;
    m_currentPath = canonical;
    if (!reload())
        return false;
    if (changed)
        emit currentPathChanged();
    m_statusMessage = QStringLiteral("%1 item%2")
                          .arg(m_entries.size())
                          .arg(m_entries.size() == 1 ? QString() : QStringLiteral("s"));
    emit statusMessageChanged();
    return true;
}

bool MokoFilePickerModel::openDirectory(int row)
{
    return validRow(row) && m_entries.at(row).directory
        && navigateTo(m_entries.at(row).path);
}

QVariantMap MokoFilePickerModel::entry(int row) const
{
    if (!validRow(row))
        return {};
    const Entry &entry = m_entries.at(row);
    return {{QStringLiteral("name"), entry.name},
            {QStringLiteral("path"), entry.path},
            {QStringLiteral("isDirectory"), entry.directory}};
}

QString MokoFilePickerModel::resolveSelection(const QString &mode,
                                              int selectedRow,
                                              const QString &fileName)
{
    if (mode == QStringLiteral("folder")) {
        QString path = m_currentPath;
        if (validRow(selectedRow) && m_entries.at(selectedRow).directory)
            path = m_entries.at(selectedRow).path;
        const QFileInfo info(path);
        if (!info.exists() || !info.isDir() || !info.isWritable()) {
            fail(QStringLiteral("Choose a writable folder."));
            return {};
        }
        return info.absoluteFilePath();
    }

    if (mode == QStringLiteral("open")) {
        if (!validRow(selectedRow) || m_entries.at(selectedRow).directory) {
            fail(QStringLiteral("Choose a readable file."));
            return {};
        }
        const QFileInfo info(m_entries.at(selectedRow).path);
        if (!info.exists() || !info.isFile() || !info.isReadable()) {
            fail(QStringLiteral("That file is not readable."));
            return {};
        }
        return info.absoluteFilePath();
    }

    if (mode != QStringLiteral("save")) {
        fail(QStringLiteral("Unsupported file dialog mode."));
        return {};
    }
    const QString name = fileName.trimmed();
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")
        || name.contains(u'/') || name.contains(u'\\')) {
        fail(QStringLiteral("Enter a file name without path separators."));
        return {};
    }
    const QFileInfo directory(m_currentPath);
    if (!directory.isDir() || !directory.isWritable()) {
        fail(QStringLiteral("This folder is not writable."));
        return {};
    }
    const QString path = QDir(m_currentPath).filePath(name);
    if (QFileInfo(path).isDir()) {
        fail(QStringLiteral("A folder already uses that name."));
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool MokoFilePickerModel::pathExists(const QString &path) const
{
    return QFileInfo::exists(path);
}

bool MokoFilePickerModel::reload()
{
    QDir directory(m_currentPath);
    if (!directory.isReadable())
        return fail(QStringLiteral("This folder cannot be read."));
    QVector<Entry> entries;
    const QFileInfoList infos = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        entries.append({info.fileName(),
                        info.absoluteFilePath(),
                        info.isDir() ? QStringLiteral("Folder") : formatSize(info.size()),
                        QLocale().toString(info.lastModified(), QLocale::ShortFormat),
                        info.isDir()});
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit countChanged();
    return true;
}

bool MokoFilePickerModel::fail(const QString &message)
{
    m_statusMessage = message;
    emit statusMessageChanged();
    return false;
}

bool MokoFilePickerModel::validRow(int row) const
{
    return row >= 0 && row < m_entries.size();
}

QString MokoFilePickerModel::formatSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}
