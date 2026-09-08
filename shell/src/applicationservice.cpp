#include "applicationservice.h"

#include "applicationregistry.h"

ApplicationService::ApplicationService(ApplicationRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
}

QVariantMap ApplicationService::openApplication(const QString &appId)
{
    const QVariantMap application = m_registry->application(appId);
    if (application.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Application is not installed.")}};
    }

    const bool launched = m_registry->launch(appId);
    return {{QStringLiteral("ok"), launched},
            {QStringLiteral("message"), launched
                 ? QStringLiteral("Application launch accepted.")
                 : QStringLiteral("Application could not be launched.")},
            {QStringLiteral("displayName"), application.value(QStringLiteral("displayName"))}};
}

QVariantMap ApplicationService::reloadApplications()
{
    m_registry->reload();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("count"), m_registry->rowCount()}};
}
