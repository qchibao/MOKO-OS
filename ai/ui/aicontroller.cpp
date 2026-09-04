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
        setConnected(false);
        setProcessing(false);
        setFailed(true);
        setResponse(QStringLiteral("The local MOKO AI daemon disconnected."));
    });
    QTimer::singleShot(0, this, &AiController::refreshConnection);
}

bool AiController::connected() const { return m_connected; }
bool AiController::processing() const { return m_processing; }
bool AiController::failed() const { return m_failed; }
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
    QDBusInterface daemon(QString::fromLatin1(serviceName),
                          QString::fromLatin1(objectPath),
                          QString::fromLatin1(interfaceName),
                          QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(daemon.asyncCall(QStringLiteral("ping")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        const QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        const bool available = !reply.isError() && reply.value() == QStringLiteral("pong");
        setConnected(available);
        if (available) {
            setFailed(false);
            if (!m_processing)
                setResponse(QStringLiteral("Local MOKO AI actions are ready."));
        }
    });
}

void AiController::submit(const QString &prompt)
{
    if (m_processing)
        return;
    const QString request = prompt.trimmed();
    if (request.isEmpty()) {
        setFailed(true);
        setResponse(QStringLiteral("Enter a request for MOKO AI."));
        return;
    }
    if (!m_connected) {
        setFailed(true);
        setResponse(QStringLiteral("The local MOKO AI daemon is not connected."));
        refreshConnection();
        return;
    }

    setProcessing(true);
    setFailed(false);
    setResponse(QStringLiteral("Processing locally..."));

    QDBusInterface daemon(QString::fromLatin1(serviceName),
                          QString::fromLatin1(objectPath),
                          QString::fromLatin1(interfaceName),
                          QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        daemon.asyncCall(QStringLiteral("request"), request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        setProcessing(false);
        if (reply.isError()) {
            setConnected(false);
            setFailed(true);
            setResponse(QStringLiteral("MOKO AI request failed: %1").arg(reply.error().message()));
            return;
        }

        const QVariantMap value = reply.value();
        const bool ok = value.value(QStringLiteral("ok")).toBool();
        setProvider(value.value(QStringLiteral("provider")).toString());
        setFailed(!ok);
        setResponse(value.value(QStringLiteral("message")).toString());
        writeUiEvent(value.value(QStringLiteral("action")).toString(), ok);
    });
}
