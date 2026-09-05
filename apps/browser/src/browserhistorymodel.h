#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QUrl>
#include <QVector>

class BrowserHistoryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        UrlRole,
        HostRole,
        VisitedRole,
        VisitedTextRole
    };
    Q_ENUM(Role)

    explicit BrowserHistoryModel(QObject *parent = nullptr, QString storagePath = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addVisit(const QString &title, const QUrl &url);
    Q_INVOKABLE QVariantMap entry(int row) const;
    Q_INVOKABLE void clear();

signals:
    void countChanged();

private:
    struct Entry {
        QString title;
        QUrl url;
        QDateTime visited;
    };

    void load();
    void save() const;

    QVector<Entry> m_entries;
    QString m_storagePath;
};
