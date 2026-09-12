#include "powerkeyinhibitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <unistd.h>

namespace {

constexpr auto logindService = "org.freedesktop.login1";
constexpr auto logindPath = "/org/freedesktop/login1";
constexpr auto logindInterface = "org.freedesktop.login1.Manager";
constexpr int dbusTimeoutMs = 1500;
constexpr int retryDelayMs = 500;

void writeLiveEvent(const QString &event)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(event.toUtf8());
    file.write("\n");
}

} // namespace

PowerKeyInhibitor::PowerKeyInhibitor(QObject *parent)
    : QObject(parent)
{
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(retryDelayMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &PowerKeyInhibitor::requestInhibitor);
}

PowerKeyInhibitor::~PowerKeyInhibitor()
{
    releaseInhibitor();
}

bool PowerKeyInhibitor::enabled() const
{
    return m_enabled;
}

bool PowerKeyInhibitor::active() const
{
    return m_inhibitorFd >= 0;
}

void PowerKeyInhibitor::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    ++m_generation;
    emit enabledChanged();
    if (!enabled) {
        m_retryTimer.stop();
        releaseInhibitor();
        writeLiveEvent(QStringLiteral("MOKO_POWER_KEY_INHIBITOR state=released uid=%1")
                           .arg(static_cast<qulonglong>(geteuid())));
        return;
    }
    requestInhibitor();
}

void PowerKeyInhibitor::requestInhibitor()
{
    if (!m_enabled || active() || m_request != nullptr)
        return;

    QDBusInterface manager(QString::fromLatin1(logindService),
                           QString::fromLatin1(logindPath),
                           QString::fromLatin1(logindInterface),
                           QDBusConnection::systemBus());
    manager.setTimeout(dbusTimeoutMs);
    if (!manager.isValid()) {
        scheduleRetry();
        return;
    }

    const quint64 generation = m_generation;
    m_request = new QDBusPendingCallWatcher(
        manager.asyncCall(QStringLiteral("Inhibit"),
                          QStringLiteral("handle-power-key"),
                          QStringLiteral("MOKO Shell"),
                          QStringLiteral("Show the MOKO power menu after a long press"),
                          QStringLiteral("block")),
        this);
    connect(m_request, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *finished;
                m_request = nullptr;
                finished->deleteLater();
                if (!m_enabled || generation != m_generation) {
                    if (m_enabled)
                        requestInhibitor();
                    return;
                }
                if (reply.isError() || !reply.value().isValid()) {
                    writeLiveEvent(QStringLiteral(
                                       "MOKO_POWER_KEY_INHIBITOR state=retry uid=%1")
                                       .arg(static_cast<qulonglong>(geteuid())));
                    scheduleRetry();
                    return;
                }

                m_inhibitorFd = ::dup(reply.value().fileDescriptor());
                if (m_inhibitorFd < 0) {
                    scheduleRetry();
                    return;
                }
                writeLiveEvent(QStringLiteral("MOKO_POWER_KEY_INHIBITOR state=ready uid=%1")
                                   .arg(static_cast<qulonglong>(geteuid())));
                emit activeChanged();
            });
}

void PowerKeyInhibitor::scheduleRetry()
{
    if (m_enabled && !active() && !m_retryTimer.isActive())
        m_retryTimer.start();
}

void PowerKeyInhibitor::releaseInhibitor()
{
    if (m_inhibitorFd < 0)
        return;
    ::close(m_inhibitorFd);
    m_inhibitorFd = -1;
    emit activeChanged();
}
