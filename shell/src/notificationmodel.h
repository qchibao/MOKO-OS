#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVariantMap>
#include <QVector>

#include <cstdint>

class NotificationModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ApplicationNameRole,
        ApplicationIconRole,
        SummaryRole,
        BodyRole,
        CreatedTextRole,
        UnreadRole,
        ActiveRole,
        ActionsRole
    };
    Q_ENUM(Role)

    explicit NotificationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int unreadCount() const;

    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE bool dismiss(int row);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE bool invokeAction(int row, const QString &actionKey);
    Q_INVOKABLE uint addLocalNotification(const QString &summary, const QString &body);
    Q_INVOKABLE void reportCenterOpened() const;

public slots:
    uint Notify(const QString &appName,
                uint replacesId,
                const QString &appIcon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int expireTimeout);
    void CloseNotification(uint id);
    QStringList GetCapabilities() const;
    void GetServerInformation(QString &name,
                              QString &vendor,
                              QString &version,
                              QString &specVersion) const;

signals:
    void countChanged();
    void unreadCountChanged();
    void notificationReceived(const QString &summary, const QString &body);
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    struct Entry {
        uint id = 0;
        QString applicationName;
        QString applicationIcon;
        QString summary;
        QString body;
        QDateTime created;
        QVariantList actions;
        bool unread = true;
        bool active = true;
        std::uint64_t revision = 0;
    };

    int rowForId(uint id) const;
    bool removeEntry(int row, uint reason);
    void expire(uint id, std::uint64_t revision);
    void trimHistory();
    static QString boundedText(const QString &value, int maximum);

    QVector<Entry> m_entries;
    uint m_nextId = 1;
    int m_unreadCount = 0;
    std::uint64_t m_revision = 0;
};
