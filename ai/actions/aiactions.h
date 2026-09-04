#pragma once

#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class AiActions final
{
public:
    using ApplicationLauncher = std::function<QVariantMap(const QString &)>;
    using FileLauncher = std::function<bool(const QString &)>;

    explicit AiActions(QString homeDirectory = {},
                       ApplicationLauncher applicationLauncher = {},
                       FileLauncher fileLauncher = {});

    QVariantMap getSystemSummary() const;
    QStringList searchFiles(const QString &query, int maximumResults = 40) const;
    QVariantMap openApplication(const QString &appId) const;
    QVariantMap openFile(const QString &path) const;
    QVariantMap getBatteryStatus() const;
    QVariantMap getNetworkStatus() const;
    QVariantMap getStorageStatus() const;

    bool isApplicationAllowed(const QString &appId) const;
    bool isPathAllowed(const QString &path) const;

private:
    static QString readTextFile(const QString &path);
    static QMap<QString, QString> readKeyValueFile(const QString &path);
    static QString formatBytes(quint64 bytes);
    static QVariantMap result(bool ok, const QString &message, const QVariant &data = {});
    static void writeActionEvent(const QString &action,
                                 const QString &target,
                                 const QString &state);

    QString canonicalAllowedPath(const QString &path) const;

    QString m_homeDirectory;
    QSet<QString> m_allowedApplications;
    ApplicationLauncher m_applicationLauncher;
    FileLauncher m_fileLauncher;
};
