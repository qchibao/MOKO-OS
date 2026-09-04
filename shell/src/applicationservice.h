#pragma once

#include <QObject>
#include <QVariantMap>

class ApplicationRegistry;

class ApplicationService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.moko.Applications1")

public:
    explicit ApplicationService(ApplicationRegistry *registry, QObject *parent = nullptr);

public slots:
    QVariantMap openApplication(const QString &appId);

private:
    ApplicationRegistry *m_registry;
};
