#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QUrl>
#include <QVector>

class QWebEngineDownloadRequest;

class BrowserDownloadModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory NOTIFY downloadDirectoryChanged)

public:
    enum Role {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole,
        SourceUrlRole,
        MimeTypeRole,
        StateRole,
        StatusTextRole,
        ReceivedBytesRole,
        TotalBytesRole,
        ProgressRole,
        CanCancelRole,
        CanOpenRole
    };
    Q_ENUM(Role)

    explicit BrowserDownloadModel(QObject *parent = nullptr, QString downloadDirectory = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString downloadDirectory() const;

    Q_INVOKABLE bool acceptDownload(QObject *downloadObject);
    Q_INVOKABLE bool cancel(int row);
    Q_INVOKABLE bool open(int row);
    Q_INVOKABLE bool showInFiles(int row = -1);
    Q_INVOKABLE bool setDownloadDirectory(const QString &directory);
    Q_INVOKABLE QVariantMap entry(int row) const;
    Q_INVOKABLE void clearFinished();

    static QString safeFileName(const QString &suggestedFileName);
    static bool isPathInside(const QString &path, const QString &directory);

signals:
    void countChanged();
    void statusMessage(const QString &message);
    void downloadCompleted(const QString &path, qint64 bytes);
    void downloadDirectoryChanged();

private:
    struct Entry {
        QPointer<QWebEngineDownloadRequest> request;
        QString fileName;
        QString filePath;
        QUrl sourceUrl;
        QString mimeType;
        QString state;
        QString statusText;
        qint64 receivedBytes = 0;
        qint64 totalBytes = -1;
        double progress = 0.0;
        bool canCancel = false;
        bool canOpen = false;
        bool completionReported = false;
    };

    int rowForRequest(const QWebEngineDownloadRequest *request) const;
    QString uniqueFileName(const QString &suggestedFileName) const;
    void updateRequest(QWebEngineDownloadRequest *request);
    bool fail(const QString &message);
    static QString formatBytes(qint64 bytes);

    QVector<Entry> m_entries;
    QString m_downloadDirectory;
};
