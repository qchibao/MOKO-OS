#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

class QMimeData;

class FileModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardMode READ clipboardMode NOTIFY clipboardChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        UrlRole,
        DirectoryRole,
        SymbolicLinkRole,
        SizeRole,
        SizeTextRole,
        ModifiedRole,
        ModifiedTextRole,
        TypeRole,
        WritableRole
    };
    Q_ENUM(Role)

    explicit FileModel(QObject *parent = nullptr, const QString &initialPath = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    QString displayPath() const;
    bool canGoBack() const;
    bool canGoForward() const;
    bool canPaste() const;
    QString clipboardMode() const;
    QString statusMessage() const;

    Q_INVOKABLE bool navigateHome();
    Q_INVOKABLE bool navigateTo(const QString &path);
    Q_INVOKABLE bool navigateUp();
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE bool goForward();
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool openEntry(int row);
    Q_INVOKABLE QVariantMap entry(int row) const;
    Q_INVOKABLE QVariantList propertiesFor(int row) const;
    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool renameEntry(int row, const QString &newName);
    Q_INVOKABLE bool stageCopy(int row);
    Q_INVOKABLE bool stageMove(int row);
    Q_INVOKABLE bool paste();
    Q_INVOKABLE QString requestDelete(int row);
    Q_INVOKABLE bool confirmDelete(const QString &token);
    Q_INVOKABLE void cancelDelete();

signals:
    void currentPathChanged();
    void historyChanged();
    void clipboardChanged();
    void statusMessageChanged();
    void countChanged();
    void errorOccurred(const QString &message);
    void deleteConfirmationRequested(const QString &name,
                                     const QString &path,
                                     const QString &token);

private:
    struct Entry {
        QString name;
        QString path;
        QString url;
        QString type;
        qint64 size = 0;
        QDateTime modified;
        bool directory = false;
        bool symbolicLink = false;
        bool writable = false;
    };

    bool navigateInternal(const QString &path, bool addHistory);
    bool reloadEntries();
    bool validRow(int row) const;
    bool validateName(const QString &name, QString *cleanName = nullptr);
    bool copyPath(const QString &source, const QString &destination, QString *error) const;
    bool removePath(const QString &path, QString *error) const;
    void stageClipboard(int row, const QString &mode);
    void readSystemClipboard();
    QStringList clipboardPaths(const QMimeData *mimeData, QString *mode) const;
    void setStatusMessage(const QString &message);
    bool fail(const QString &message);
    static QString formatSize(qint64 bytes);

    QVector<Entry> m_entries;
    QString m_currentPath;
    QStringList m_history;
    int m_historyIndex = -1;
    QStringList m_clipboardSources;
    QString m_clipboardMode;
    QString m_pendingDeletePath;
    QString m_pendingDeleteToken;
    QString m_statusMessage;
};
