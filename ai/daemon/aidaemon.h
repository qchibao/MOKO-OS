#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <memory>

class AiActions;
class AiProvider;

class AiDaemon final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.moko.AI1")

public:
    AiDaemon(AiActions *actions, std::unique_ptr<AiProvider> provider, QObject *parent = nullptr);
    ~AiDaemon() override;

public slots:
    QString ping() const;
    QVariantMap getSystemSummary() const;
    QStringList searchFiles(const QString &query) const;
    QVariantMap openApplication(const QString &appId) const;
    QVariantMap openFile(const QString &path) const;
    QVariantMap getBatteryStatus() const;
    QVariantMap getNetworkStatus() const;
    QVariantMap getStorageStatus() const;
    QVariantMap request(const QString &prompt) const;

private:
    QVariantMap statusResponse(const QString &action,
                               const QString &intro,
                               const QVariantMap &data) const;

    AiActions *m_actions;
    std::unique_ptr<AiProvider> m_provider;
};
