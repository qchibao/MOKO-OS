#include "browserhistorymodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {

constexpr int maximumHistoryEntries = 250;

QUrl historyUrl(QUrl url)
{
    url.setUserInfo({});
    return url;
}

} // namespace

BrowserHistoryModel::BrowserHistoryModel(QObject *parent, QString storagePath)
    : QAbstractListModel(parent)
    , m_storagePath(std::move(storagePath))
{
    if (m_storagePath.isEmpty()) {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_storagePath = QDir(dataDirectory).filePath(QStringLiteral("history.json"));
    }
    load();
}

int BrowserHistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant BrowserHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case UrlRole:
        return entry.url;
    case HostRole:
        return entry.url.host();
    case VisitedRole:
        return entry.visited;
    case VisitedTextRole:
        return entry.visited.toLocalTime().toString(QStringLiteral("MMM d, HH:mm"));
    default:
        return {};
    }
}

QHash<int, QByteArray> BrowserHistoryModel::roleNames() const
{
    return {{TitleRole, "title"},
            {UrlRole, "entryUrl"},
            {HostRole, "host"},
            {VisitedRole, "visited"},
            {VisitedTextRole, "visitedText"}};
}

void BrowserHistoryModel::addVisit(const QString &title, const QUrl &url)
{
    const QUrl cleanUrl = historyUrl(url);
    const QString scheme = cleanUrl.scheme().toLower();
    if (!cleanUrl.isValid() || cleanUrl.host().isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return;
    }

    const auto existing = std::find_if(m_entries.begin(), m_entries.end(),
                                       [&cleanUrl](const Entry &entry) {
        return entry.url == cleanUrl;
    });
    if (existing != m_entries.end()) {
        const int row = std::distance(m_entries.begin(), existing);
        beginRemoveRows({}, row, row);
        m_entries.removeAt(row);
        endRemoveRows();
    }

    beginInsertRows({}, 0, 0);
    m_entries.prepend({title.trimmed().isEmpty() ? cleanUrl.host() : title.trimmed(),
                       cleanUrl,
                       QDateTime::currentDateTimeUtc()});
    endInsertRows();

    if (m_entries.size() > maximumHistoryEntries) {
        beginRemoveRows({}, maximumHistoryEntries, m_entries.size() - 1);
        m_entries.resize(maximumHistoryEntries);
        endRemoveRows();
    }
    emit countChanged();
    save();
}

QVariantMap BrowserHistoryModel::entry(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(row);
    return {{QStringLiteral("title"), entry.title},
            {QStringLiteral("url"), entry.url},
            {QStringLiteral("host"), entry.url.host()},
            {QStringLiteral("visited"), entry.visited}};
}

void BrowserHistoryModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    save();
}

void BrowserHistoryModel::load()
{
    QFile file(m_storagePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray())
        return;

    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        const QUrl url(object.value(QStringLiteral("url")).toString());
        const QDateTime visited = QDateTime::fromString(
            object.value(QStringLiteral("visited")).toString(), Qt::ISODateWithMs);
        const QString scheme = url.scheme().toLower();
        if (!url.isValid() || url.host().isEmpty() || !visited.isValid()
            || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
            continue;
        }
        m_entries.append({object.value(QStringLiteral("title")).toString(), url, visited});
        if (m_entries.size() >= maximumHistoryEntries)
            break;
    }
}

void BrowserHistoryModel::save() const
{
    QDir().mkpath(QFileInfo(m_storagePath).absolutePath());
    QJsonArray values;
    for (const Entry &entry : m_entries) {
        values.append(QJsonObject{{QStringLiteral("title"), entry.title},
                                  {QStringLiteral("url"), entry.url.toString()},
                                  {QStringLiteral("visited"),
                                   entry.visited.toString(Qt::ISODateWithMs)}});
    }
    QSaveFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(values).toJson(QJsonDocument::Compact));
    file.commit();
}
