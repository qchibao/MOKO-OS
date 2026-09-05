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

QVariantMap AiDaemon::providerStatus() const
{
    const bool available = m_provider->isAvailable();
    const QString unavailableMessage = m_provider->unavailableMessage().isEmpty()
        ? QStringLiteral("The selected MOKO AI provider is not configured.")
        : m_provider->unavailableMessage();
    return {{QStringLiteral("name"), m_provider->name()},
            {QStringLiteral("available"), available},
            {QStringLiteral("message"), available
                 ? QStringLiteral("Safe local MOKO AI actions are ready.")
                 : unavailableMessage}};
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
        detail = QStringLiteral("%1 on %2, kernel %3, with %4 memory available.")
                     .arg(data.value(QStringLiteral("base")).toString(),
                          data.value(QStringLiteral("architecture")).toString(),
                          data.value(QStringLiteral("kernel")).toString(),
                          data.value(QStringLiteral("memoryAvailable")).toString());
    } else if (action == QStringLiteral("battery_status")) {
        const QVariantList batteries = data.value(QStringLiteral("batteries")).toList();
        if (batteries.isEmpty()) {
            detail = QStringLiteral("No battery is detected on this system.");
        } else {
            const QVariantMap battery = batteries.constFirst().toMap();
            const QString capacity = battery.value(QStringLiteral("capacity")).toString();
            const QString state = battery.value(QStringLiteral("status")).toString();
            detail = capacity.isEmpty()
                ? QStringLiteral("The battery reports %1.").arg(state.toLower())
                : QStringLiteral("The battery is at %1% and reports %2.")
                      .arg(capacity, state.toLower());
        }
    } else if (action == QStringLiteral("network_status")) {
        detail = data.value(QStringLiteral("available")).toBool()
            ? QStringLiteral("The network is %1 with %2 device(s); Wi-Fi is %3.")
                  .arg(data.value(QStringLiteral("state")).toString())
                  .arg(data.value(QStringLiteral("deviceCount")).toInt())
                  .arg(data.value(QStringLiteral("wirelessEnabled")).toBool()
                           ? QStringLiteral("on") : QStringLiteral("off"))
            : QStringLiteral("NetworkManager is unavailable.");
    } else if (action == QStringLiteral("storage_status")) {
        const QVariantList volumes = data.value(QStringLiteral("volumes")).toList();
        QVariantMap primary;
        for (const QVariant &volumeValue : volumes) {
            const QVariantMap volume = volumeValue.toMap();
            if (primary.isEmpty() || volume.value(QStringLiteral("mountPoint")).toString()
                                         == QStringLiteral("/")) {
                primary = volume;
            }
        }
        detail = primary.isEmpty()
            ? QStringLiteral("No mounted filesystem capacity is available.")
            : QStringLiteral("%1 is available of %2 on %3.")
                  .arg(primary.value(QStringLiteral("available")).toString(),
                       primary.value(QStringLiteral("total")).toString(),
                       primary.value(QStringLiteral("mountPoint")).toString());
    }
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("action"), action},
            {QStringLiteral("message"), intro + u' ' + detail},
            {QStringLiteral("data"), data},
            {QStringLiteral("provider"), m_provider->name()}};
}

QVariantMap AiDaemon::request(const QString &prompt) const
{
    if (!m_provider->isAvailable()) {
        const QString message = m_provider->unavailableMessage().isEmpty()
            ? QStringLiteral("The selected MOKO AI provider is not configured.")
            : m_provider->unavailableMessage();
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("action"), QStringLiteral("provider_unavailable")},
                {QStringLiteral("message"), message},
                {QStringLiteral("provider"), m_provider->name()}};
    }
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
