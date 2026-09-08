#include "notificationmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTimer>
#include <QSettings>
#include <QTimeZone>

#include <algorithm>

#include <unistd.h>

namespace {

constexpr int maximumHistory = 100;

void writeLiveEvent(const QString &event)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(event.toUtf8());
    file.write("\n");
}

}

NotificationModel::NotificationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NotificationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant NotificationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case ApplicationNameRole:
        return entry.applicationName;
    case ApplicationIconRole:
        return entry.applicationIcon;
    case SummaryRole:
        return entry.summary;
    case BodyRole:
        return entry.body;
    case CreatedTextRole:
    {
        const QSettings settings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"));
        const QString configured = settings.value(QStringLiteral("dateTime/timeZone")).toString();
        const QTimeZone zone(QTimeZone::isTimeZoneIdAvailable(configured.toUtf8())
                                 ? configured.toUtf8() : QTimeZone::systemTimeZoneId());
        return QLocale().toString(entry.created.toTimeZone(zone).time(), QLocale::ShortFormat);
    }
    case UnreadRole:
        return entry.unread;
    case ActiveRole:
        return entry.active;
    case ActionsRole:
        return entry.actions;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {{IdRole, "notificationId"},
            {ApplicationNameRole, "applicationName"},
            {ApplicationIconRole, "applicationIcon"},
            {SummaryRole, "summary"},
            {BodyRole, "body"},
            {CreatedTextRole, "createdText"},
            {UnreadRole, "unread"},
            {ActiveRole, "active"},
            {ActionsRole, "actions"}};
}

int NotificationModel::unreadCount() const
{
    return m_unreadCount;
}

void NotificationModel::markAllRead()
{
    if (m_unreadCount == 0)
        return;
    for (Entry &entry : m_entries)
        entry.unread = false;
    m_unreadCount = 0;
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(m_entries.size() - 1), {UnreadRole});
    emit unreadCountChanged();
}

bool NotificationModel::dismiss(int row)
{
    return removeEntry(row, 2);
}

void NotificationModel::clearAll()
{
    if (m_entries.isEmpty())
        return;
    const QVector<Entry> entries = m_entries;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    m_unreadCount = 0;
    emit countChanged();
    emit unreadCountChanged();
    for (const Entry &entry : entries) {
        if (entry.active)
            emit NotificationClosed(entry.id, 2);
    }
}

bool NotificationModel::invokeAction(int row, const QString &actionKey)
{
    if (row < 0 || row >= m_entries.size() || actionKey.isEmpty())
        return false;
    const Entry &entry = m_entries.at(row);
    const bool known = std::any_of(entry.actions.cbegin(), entry.actions.cend(),
                                   [&actionKey](const QVariant &action) {
        return action.toMap().value(QStringLiteral("key")).toString() == actionKey;
    });
    if (!known)
        return false;
    emit ActionInvoked(entry.id, actionKey);
    return true;
}

uint NotificationModel::addLocalNotification(const QString &summary, const QString &body)
{
    return Notify(QStringLiteral("MOKO OS"), 0, QString(), summary, body,
                  {}, {}, 5000);
}

void NotificationModel::reportCenterOpened() const
{
    writeLiveEvent(QStringLiteral("MOKO_NOTIFICATION_CENTER state=open count=%1 unread=%2 uid=%3")
                       .arg(m_entries.size())
                       .arg(m_unreadCount)
                       .arg(static_cast<qulonglong>(geteuid())));
}

uint NotificationModel::Notify(const QString &appName,
                               uint replacesId,
                               const QString &appIcon,
                               const QString &summary,
                               const QString &body,
                               const QStringList &actions,
                               const QVariantMap &hints,
                               int expireTimeout)
{
    Q_UNUSED(hints)
    int row = replacesId == 0 ? -1 : rowForId(replacesId);
    Entry entry;
    if (row >= 0) {
        entry = m_entries.at(row);
        if (!entry.unread)
            ++m_unreadCount;
    } else {
        entry.id = m_nextId++;
        if (m_nextId == 0)
            m_nextId = 1;
        ++m_unreadCount;
    }
    entry.applicationName = boundedText(appName.trimmed(), 120);
    if (entry.applicationName.isEmpty())
        entry.applicationName = QStringLiteral("Application");
    entry.applicationIcon = boundedText(appIcon.trimmed(), 512);
    entry.summary = boundedText(summary.trimmed(), 240);
    if (entry.summary.isEmpty())
        entry.summary = entry.applicationName;
    entry.body = boundedText(body.trimmed(), 2000);
    entry.created = QDateTime::currentDateTime();
    entry.unread = true;
    entry.active = true;
    entry.revision = ++m_revision;
    if (m_revision == 0)
        entry.revision = ++m_revision;
    entry.actions.clear();
    for (int index = 0; index + 1 < actions.size(); index += 2) {
        const QString key = boundedText(actions.at(index), 80);
        const QString label = boundedText(actions.at(index + 1), 120);
        if (!key.isEmpty() && !label.isEmpty()) {
            entry.actions.append(QVariantMap{{QStringLiteral("key"), key},
                                             {QStringLiteral("label"), label}});
        }
    }

    if (row >= 0) {
        m_entries[row] = entry;
        emit dataChanged(index(row), index(row));
    } else {
        beginInsertRows({}, 0, 0);
        m_entries.prepend(entry);
        endInsertRows();
        emit countChanged();
        trimHistory();
    }
    emit unreadCountChanged();
    emit notificationReceived(entry.summary, entry.body);

    const int timeout = expireTimeout < 0 ? 5000 : expireTimeout;
    if (timeout > 0) {
        QTimer::singleShot(timeout, this, [this, id = entry.id, revision = entry.revision]() {
            expire(id, revision);
        });
    }
    return entry.id;
}

void NotificationModel::CloseNotification(uint id)
{
    const int row = rowForId(id);
    if (row >= 0)
        removeEntry(row, 3);
}

QStringList NotificationModel::GetCapabilities() const
{
    return {QStringLiteral("actions"), QStringLiteral("body")};
}

void NotificationModel::GetServerInformation(QString &name,
                                             QString &vendor,
                                             QString &version,
                                             QString &specVersion) const
{
    name = QStringLiteral("MOKO Notification Center");
    vendor = QStringLiteral("MOKO");
    version = QStringLiteral("0.1.1");
    specVersion = QStringLiteral("1.2");
}

int NotificationModel::rowForId(uint id) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == id)
            return row;
    }
    return -1;
}

bool NotificationModel::removeEntry(int row, uint reason)
{
    if (row < 0 || row >= m_entries.size())
        return false;
    const Entry entry = m_entries.at(row);
    beginRemoveRows({}, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    if (entry.unread) {
        --m_unreadCount;
        emit unreadCountChanged();
    }
    emit countChanged();
    if (entry.active)
        emit NotificationClosed(entry.id, reason);
    return true;
}

void NotificationModel::expire(uint id, std::uint64_t revision)
{
    const int row = rowForId(id);
    if (row < 0 || !m_entries.at(row).active || m_entries.at(row).revision != revision)
        return;
    m_entries[row].active = false;
    emit dataChanged(index(row), index(row), {ActiveRole});
    emit NotificationClosed(id, 1);
}

void NotificationModel::trimHistory()
{
    if (m_entries.size() <= maximumHistory)
        return;
    const int first = maximumHistory;
    const int last = m_entries.size() - 1;
    for (int row = first; row <= last; ++row) {
        if (m_entries.at(row).unread)
            --m_unreadCount;
    }
    beginRemoveRows({}, first, last);
    m_entries.erase(m_entries.begin() + first, m_entries.end());
    endRemoveRows();
    emit countChanged();
    emit unreadCountChanged();
}

QString NotificationModel::boundedText(const QString &value, int maximum)
{
    return value.left(maximum);
}
