#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QProcess>
#include <QSortFilterProxyModel>

struct ApplicationEntry
{
    QString id;
    QString displayName;
    QString iconName;
    QString iconSource;
    QString executable;
    QString program;
    QStringList arguments;
    QString category;
    QString description;
    QStringList keywords;
    QString desktopFile;
    QString workingDirectory;
    QString glyph;
    QString launchState = QStringLiteral("ready");
    QString launchMessage;
    bool terminal = false;
    bool pinned = false;
    int pinOrder = 1000;
};

class ApplicationRegistry final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        AppIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconNameRole,
        IconSourceRole,
        ExecutableRole,
        CategoryRole,
        DescriptionRole,
        KeywordsRole,
        GlyphRole,
        LaunchStateRole,
        LaunchMessageRole,
        PinnedRole,
        PinOrderRole
    };
    Q_ENUM(Role)

    explicit ApplicationRegistry(QObject *parent = nullptr, QStringList applicationDirectories = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool launch(const QString &appId);
    Q_INVOKABLE QVariantMap application(const QString &appId) const;
    Q_INVOKABLE void reload();

    int indexOfApplication(const QString &appId) const;

signals:
    void countChanged();
    void applicationLaunching(const QString &appId, const QString &displayName);
    void applicationRunning(const QString &appId, const QString &displayName);
    void applicationFailed(const QString &appId, const QString &displayName, const QString &message);

private:
    void setLaunchState(const QString &appId, const QString &state, const QString &message = {});
    QStringList defaultApplicationDirectories() const;

    QVector<ApplicationEntry> m_entries;
    QStringList m_applicationDirectories;
    QHash<QString, QPointer<QProcess>> m_processes;
};

class ApplicationFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool pinnedOnly READ pinnedOnly WRITE setPinnedOnly NOTIFY pinnedOnlyChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit ApplicationFilterModel(QObject *parent = nullptr);

    QString searchText() const;
    void setSearchText(const QString &searchText);
    bool pinnedOnly() const;
    void setPinnedOnly(bool pinnedOnly);

signals:
    void searchTextChanged();
    void pinnedOnlyChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_searchText;
    bool m_pinnedOnly = false;
};
