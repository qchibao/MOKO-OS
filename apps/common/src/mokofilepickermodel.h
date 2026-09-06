#pragma once

#include <QAbstractListModel>
#include <QVector>

class MokoFilePickerModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        DirectoryRole,
        SizeTextRole,
        ModifiedTextRole
    };
    Q_ENUM(Role)

    explicit MokoFilePickerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString currentPath() const;
    QString displayPath() const;
    QString statusMessage() const;

    Q_INVOKABLE bool navigateHome();
    Q_INVOKABLE bool navigateUp();
    Q_INVOKABLE bool navigateTo(const QString &path);
    Q_INVOKABLE bool openDirectory(int row);
    Q_INVOKABLE QVariantMap entry(int row) const;
    Q_INVOKABLE QString resolveSelection(const QString &mode,
                                         int selectedRow,
                                         const QString &fileName);
    Q_INVOKABLE bool pathExists(const QString &path) const;

signals:
    void currentPathChanged();
    void statusMessageChanged();
    void countChanged();

private:
    struct Entry {
        QString name;
        QString path;
        QString sizeText;
        QString modifiedText;
        bool directory = false;
    };

    bool reload();
    bool fail(const QString &message);
    bool validRow(int row) const;
    static QString formatSize(qint64 bytes);

    QVector<Entry> m_entries;
    QString m_currentPath;
    QString m_statusMessage;
};
