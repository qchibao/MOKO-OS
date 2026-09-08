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
    Q_PROPERTY(QVariantList quickSites READ quickSites NOTIFY quickSitesChanged)

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
    QVariantList quickSites() const;

    Q_INVOKABLE QUrl urlFromInput(const QString &input) const;
    Q_INVOKABLE QUrl localFileUrl(const QString &path) const;
    Q_INVOKABLE void recordLoad(const QString &title, const QUrl &url,
                                bool succeeded, const QString &error = {});
    Q_INVOKABLE void reportJavaScript(const QUrl &url, const QVariant &result);
    Q_INVOKABLE bool showDownloadsInFiles();
    Q_INVOKABLE bool browserHistorySwipeEnabled() const;
    Q_INVOKABLE bool addQuickSite(const QString &label, const QString &url);
    Q_INVOKABLE bool updateQuickSite(int index, const QString &label, const QString &url);
    Q_INVOKABLE bool removeQuickSite(int index);
    Q_INVOKABLE bool moveQuickSite(int from, int to);

signals:
    void statusMessageChanged();
    void quickSitesChanged();
    void javaScriptValidated(const QUrl &url, bool passed);

private:
    void setStatusMessage(const QString &message);
    void loadQuickSites();
    bool setQuickSite(int index, const QString &label, const QString &url);
    static QString markerValue(QString value);

    BrowserHistoryModel m_history;
    BrowserDownloadModel m_downloads;
    QUrl m_initialUrl;
    QUrl m_validationDownloadUrl;
    bool m_smokeTest = false;
    QString m_statusMessage;
    QVariantList m_quickSites;
};
