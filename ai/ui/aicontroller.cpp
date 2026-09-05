#include "aicontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <unistd.h>

namespace {

constexpr auto serviceName = "org.moko.AI1";
constexpr auto objectPath = "/org/moko/AI1";
constexpr auto interfaceName = "org.moko.AI1";

QString sanitizeEventValue(QString value)
{
    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_') {
            character = u'_';
        }
    }
    return value.left(120);
}

void writeUiEvent(const QString &action, bool ok)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_AI_UI state=response action=%1 ok=%2 uid=%3\n")
                   .arg(sanitizeEventValue(action))
                   .arg(ok ? 1 : 0)
                   .arg(geteuid())
                   .toUtf8());
    file.flush();
}

void writeConnectedEvent()
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_AI_UI state=connected uid=%1\n").arg(geteuid()).toUtf8());
    file.flush();
}

void writeStateEvent(const QString &state, const QString &provider)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_AI_UI state=%1 provider=%2 uid=%3\n")
                   .arg(sanitizeEventValue(state.toLower().replace(u' ', u'_')),
                        sanitizeEventValue(provider.isEmpty()
                                               ? QStringLiteral("none")
                                               : provider))
                   .arg(geteuid())
                   .toUtf8());
    file.flush();
}

} // namespace

AiController::AiController(QObject *parent)
    : QObject(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(QString::fromLatin1(serviceName),
                                               QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForRegistration
                                                   | QDBusServiceWatcher::WatchForUnregistration,
                                               this))
{
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this]() { refreshConnection(); });
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        ++m_connectionGeneration;
        ++m_requestGeneration;
        setConnected(false);
        setProcessing(false);
        setProviderAvailable(false);
        setFailed(true);
        setState(QStringLiteral("Provider unavailable"));
        setResponse(QStringLiteral("The local MOKO AI daemon disconnected."));
    });
    QTimer::singleShot(0, this, &AiController::refreshConnection);
}

bool AiController::connected() const { return m_connected; }
bool AiController::processing() const { return m_processing; }
bool AiController::failed() const { return m_failed; }
bool AiController::providerAvailable() const { return m_providerAvailable; }
QString AiController::state() const { return m_state; }
QString AiController::response() const { return m_response; }
QString AiController::provider() const { return m_provider; }

void AiController::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
    if (connected)
        writeConnectedEvent();
}

void AiController::setProcessing(bool processing)
{
    if (m_processing == processing)
        return;
    m_processing = processing;
    emit processingChanged();
}

void AiController::setFailed(bool failed)
{
    if (m_failed == failed)
        return;
    m_failed = failed;
    emit failedChanged();
}

void AiController::setProviderAvailable(bool available)
{
    if (m_providerAvailable == available)
        return;
    m_providerAvailable = available;
    emit providerAvailableChanged();
}

void AiController::setState(const QString &state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    writeStateEvent(state, m_provider);
}

void AiController::setResponse(const QString &response)
{
    if (m_response == response)
        return;
    m_response = response;
    emit responseChanged();
}

void AiController::setProvider(const QString &provider)
{
    if (m_provider == provider)
        return;
    m_provider = provider;
    emit providerChanged();
}

void AiController::refreshConnection()
{
    const quint64 generation = ++m_connectionGeneration;
    QDBusInterface daemon(QString::fromLatin1(serviceName),
                          QString::fromLatin1(objectPath),
                          QString::fromLatin1(interfaceName),
                          QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(daemon.asyncCall(QStringLiteral("ping")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        const QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_connectionGeneration)
            return;
        const bool available = !reply.isError() && reply.value() == QStringLiteral("pong");
        setConnected(available);
        if (!available) {
            setProviderAvailable(false);
            setFailed(true);
            setState(QStringLiteral("Provider unavailable"));
            setResponse(QStringLiteral("The local MOKO AI daemon is unavailable."));
            return;
        }

        QDBusInterface daemon(QString::fromLatin1(serviceName),
                              QString::fromLatin1(objectPath),
                              QString::fromLatin1(interfaceName),
                              QDBusConnection::sessionBus());
        auto *providerWatcher = new QDBusPendingCallWatcher(
            daemon.asyncCall(QStringLiteral("providerStatus")), this);
        connect(providerWatcher, &QDBusPendingCallWatcher::finished,
                this, [this, providerWatcher, generation]() {
            const QDBusPendingReply<QVariantMap> providerReply = *providerWatcher;
            providerWatcher->deleteLater();
            if (generation != m_connectionGeneration)
                return;
            if (providerReply.isError()) {
                setProviderAvailable(false);
                setFailed(true);
                setState(QStringLiteral("Provider unavailable"));
                setResponse(QStringLiteral("The MOKO AI provider could not be reached."));
                return;
            }
            const QVariantMap value = providerReply.value();
            setProvider(value.value(QStringLiteral("name")).toString());
            const bool providerAvailable = value.value(QStringLiteral("available")).toBool();
            setProviderAvailable(providerAvailable);
            if (providerAvailable) {
                if (!m_processing) {
                    setFailed(false);
                    setState(QStringLiteral("Ready"));
                    setResponse(value.value(QStringLiteral("message")).toString());
                }
            } else {
                ++m_requestGeneration;
                setProcessing(false);
                setFailed(true);
                setState(QStringLiteral("Provider unavailable"));
                setResponse(value.value(QStringLiteral("message")).toString());
            }
        });
    });
}

void AiController::submit(const QString &prompt)
{
    if (m_processing)
        return;
    const QString request = prompt.trimmed();
    if (request.isEmpty()) {
        setFailed(true);
        setState(QStringLiteral("Failed"));
        setResponse(QStringLiteral("Enter a request for MOKO AI."));
        return;
    }
    if (!m_connected || !m_providerAvailable) {
        setFailed(true);
        setState(QStringLiteral("Provider unavailable"));
        setResponse(QStringLiteral("A MOKO AI provider is not available."));
        refreshConnection();
        return;
    }

    setProcessing(true);
    setFailed(false);
    setState(QStringLiteral("Processing"));
    setResponse(QStringLiteral("Processing locally..."));
    const quint64 generation = ++m_requestGeneration;

    QDBusInterface daemon(QString::fromLatin1(serviceName),
                          QString::fromLatin1(objectPath),
                          QString::fromLatin1(interfaceName),
                          QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        daemon.asyncCall(QStringLiteral("request"), request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_requestGeneration)
            return;
        setProcessing(false);
        if (reply.isError()) {
            setConnected(false);
            setProviderAvailable(false);
            setFailed(true);
            setState(QStringLiteral("Failed"));
            setResponse(QStringLiteral("MOKO AI request failed: %1").arg(reply.error().message()));
            return;
        }

        const QVariantMap value = reply.value();
        const bool ok = value.value(QStringLiteral("ok")).toBool();
        setProvider(value.value(QStringLiteral("provider")).toString());
        setFailed(!ok);
        setProviderAvailable(value.value(QStringLiteral("action")).toString()
                             != QStringLiteral("provider_unavailable"));
        setState(ok ? QStringLiteral("Response")
                    : m_providerAvailable ? QStringLiteral("Failed")
                                          : QStringLiteral("Provider unavailable"));
        setResponse(value.value(QStringLiteral("message")).toString());
        writeUiEvent(value.value(QStringLiteral("action")).toString(), ok);
    });
}
