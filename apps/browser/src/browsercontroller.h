#pragma once

#include "browserdownloadmodel.h"
#include "browserhistorymodel.h"

#include <QObject>
#include <QUrl>

class BrowserController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl initialUrl READ initialUrl CONSTANT)
    Q_PROPERTY(QUrl validationDownloadUrl READ validationDownloadUrl CONSTANT)
    Q_PROPERTY(bool smokeTest READ smokeTest CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit BrowserController(QString initialInput = {}, bool smokeTest = false,
                               QUrl validationDownloadUrl = {},
                               QObject *parent = nullptr);

    BrowserHistoryModel *historyModel();
    BrowserDownloadModel *downloadModel();
    QUrl initialUrl() const;
    QUrl validationDownloadUrl() const;
    bool smokeTest() const;
    QString statusMessage() const;

    Q_INVOKABLE QUrl urlFromInput(const QString &input) const;
    Q_INVOKABLE void recordLoad(const QString &title, const QUrl &url,
                                bool succeeded, const QString &error = {});
    Q_INVOKABLE void reportJavaScript(const QUrl &url, const QVariant &result);
    Q_INVOKABLE bool showDownloadsInFiles();

signals:
    void statusMessageChanged();
    void javaScriptValidated(const QUrl &url, bool passed);

private:
    void setStatusMessage(const QString &message);
    static QString markerValue(QString value);

    BrowserHistoryModel m_history;
    BrowserDownloadModel m_downloads;
    QUrl m_initialUrl;
    QUrl m_validationDownloadUrl;
    bool m_smokeTest = false;
    QString m_statusMessage;
};
