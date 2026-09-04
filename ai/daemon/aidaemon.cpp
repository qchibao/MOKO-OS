#include "aidaemon.h"

#include "aiactions.h"
#include "aiprovider.h"

AiDaemon::AiDaemon(AiActions *actions,
                   std::unique_ptr<AiProvider> provider,
                   QObject *parent)
    : QObject(parent)
    , m_actions(actions)
    , m_provider(std::move(provider))
{
}

AiDaemon::~AiDaemon() = default;

QString AiDaemon::ping() const
{
    return QStringLiteral("pong");
}

QVariantMap AiDaemon::getSystemSummary() const
{
    return m_actions->getSystemSummary();
}

QStringList AiDaemon::searchFiles(const QString &query) const
{
    return m_actions->searchFiles(query);
}

QVariantMap AiDaemon::openApplication(const QString &appId) const
{
    return m_actions->openApplication(appId);
}

QVariantMap AiDaemon::openFile(const QString &path) const
{
    return m_actions->openFile(path);
}

QVariantMap AiDaemon::getBatteryStatus() const
{
    return m_actions->getBatteryStatus();
}

QVariantMap AiDaemon::getNetworkStatus() const
{
    return m_actions->getNetworkStatus();
}

QVariantMap AiDaemon::getStorageStatus() const
{
    return m_actions->getStorageStatus();
}

QVariantMap AiDaemon::statusResponse(const QString &action,
                                     const QString &intro,
                                     const QVariantMap &data) const
{
    QString detail;
    if (action == QStringLiteral("system_summary")) {
        detail = QStringLiteral("%1, kernel %2, %3 memory available.")
                     .arg(data.value(QStringLiteral("base")).toString(),
                          data.value(QStringLiteral("kernel")).toString(),
                          data.value(QStringLiteral("memoryAvailable")).toString());
    } else if (action == QStringLiteral("battery_status")) {
        detail = data.value(QStringLiteral("present")).toBool()
            ? QStringLiteral("A battery is detected; live capacity data is available.")
            : QStringLiteral("No battery is detected on this system.");
    } else if (action == QStringLiteral("network_status")) {
        detail = data.value(QStringLiteral("available")).toBool()
            ? QStringLiteral("NetworkManager reports %1 with %2 device(s).")
                  .arg(data.value(QStringLiteral("state")).toString())
                  .arg(data.value(QStringLiteral("deviceCount")).toInt())
            : QStringLiteral("NetworkManager is unavailable.");
    } else if (action == QStringLiteral("storage_status")) {
        detail = QStringLiteral("%1 mounted filesystem(s) reported.")
                     .arg(data.value(QStringLiteral("volumes")).toList().size());
    }
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("action"), action},
            {QStringLiteral("message"), intro + u' ' + detail},
            {QStringLiteral("data"), data},
            {QStringLiteral("provider"), m_provider->name()}};
}

QVariantMap AiDaemon::request(const QString &prompt) const
{
    if (prompt.trimmed().isEmpty() || prompt.size() > 512) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("action"), QStringLiteral("invalid")},
                {QStringLiteral("message"), QStringLiteral("Enter a short request for MOKO AI.")},
                {QStringLiteral("provider"), m_provider->name()}};
    }

    const AiIntent intent = m_provider->interpret(prompt);
    if (!intent.valid || intent.action == QStringLiteral("unsupported")
        || intent.action == QStringLiteral("refuse")) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("action"), intent.action.isEmpty()
                     ? QStringLiteral("unsupported")
                     : intent.action},
                {QStringLiteral("message"), intent.response},
                {QStringLiteral("provider"), m_provider->name()}};
    }

    if (intent.action == QStringLiteral("open_application")) {
        QVariantMap response = openApplication(intent.parameters.value(QStringLiteral("appId")).toString());
        response.insert(QStringLiteral("action"), intent.action);
        response.insert(QStringLiteral("provider"), m_provider->name());
        if (response.value(QStringLiteral("ok")).toBool())
            response.insert(QStringLiteral("message"), intent.response);
        return response;
    }
    if (intent.action == QStringLiteral("system_summary"))
        return statusResponse(intent.action, intent.response, getSystemSummary());
    if (intent.action == QStringLiteral("battery_status"))
        return statusResponse(intent.action, intent.response, getBatteryStatus());
    if (intent.action == QStringLiteral("network_status"))
        return statusResponse(intent.action, intent.response, getNetworkStatus());
    if (intent.action == QStringLiteral("storage_status"))
        return statusResponse(intent.action, intent.response, getStorageStatus());
    if (intent.action == QStringLiteral("search_files")) {
        const QString query = intent.parameters.value(QStringLiteral("query")).toString();
        const QStringList matches = searchFiles(query);
        return {{QStringLiteral("ok"), true},
                {QStringLiteral("action"), intent.action},
                {QStringLiteral("message"), QStringLiteral("Found %1 matching item(s) in your home directory.")
                                                      .arg(matches.size())},
                {QStringLiteral("data"), matches},
                {QStringLiteral("provider"), m_provider->name()}};
    }

    return {{QStringLiteral("ok"), false},
            {QStringLiteral("action"), QStringLiteral("denied")},
            {QStringLiteral("message"), QStringLiteral("The requested action is not allowed.")},
            {QStringLiteral("provider"), m_provider->name()}};
}
