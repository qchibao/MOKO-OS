#include "sessionlifecycle.h"

#include <QDBusConnection>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <utility>

#include <unistd.h>

namespace {

constexpr auto logindService = "org.freedesktop.login1";
constexpr auto logindPath = "/org/freedesktop/login1";
constexpr auto logindInterface = "org.freedesktop.login1.Manager";

qint64 monotonicMilliseconds()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer value;
        value.start();
        return value;
    }();
    return timer.elapsed();
}

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
    file.flush();
}

int flag(bool value)
{
    return value ? 1 : 0;
}

} // namespace

SessionLifecycle::SessionLifecycle(Hooks hooks, QObject *parent)
    : SessionLifecycle(std::move(hooks), Options{}, parent)
{
}

SessionLifecycle::SessionLifecycle(Hooks hooks, Options options, QObject *parent)
    : QObject(parent)
    , m_hooks(std::move(hooks))
    , m_options(options)
{
    m_options.healthTimeoutMs = qMax(0, m_options.healthTimeoutMs);
    m_options.healthCheckIntervalMs = qMax(1, m_options.healthCheckIntervalMs);
    if (m_options.observeLogind) {
        QDBusConnection::systemBus().connect(QString::fromLatin1(logindService),
                                             QString::fromLatin1(logindPath),
                                             QString::fromLatin1(logindInterface),
                                             QStringLiteral("PrepareForSleep"),
                                             this,
                                             SLOT(handlePrepareForSleep(bool)));
    }
}

bool SessionLifecycle::preparingForSleep() const
{
    return m_preparingForSleep;
}

void SessionLifecycle::handlePrepareForSleep(bool preparing)
{
    if (preparing) {
        ++m_resumeGeneration;
        m_preparingForSleep = true;
        m_beforeSleep = m_hooks.readState ? m_hooks.readState() : State{};
        emit preparingForSleepChanged();
        writePreparingEvent();
        return;
    }

    ++m_resumeGeneration;
    if (m_preparingForSleep) {
        m_preparingForSleep = false;
        emit preparingForSleepChanged();
    }
    writeResumedEvent();
    emit resumed();
    m_resumeStartedAtMs = monotonicMilliseconds();
    refreshAfterResume();
    const quint64 generation = m_resumeGeneration;
    QTimer::singleShot(m_options.healthCheckIntervalMs, this, [this, generation] {
        evaluateResumeHealth(generation);
    });
}

void SessionLifecycle::refreshAfterResume()
{
    if (m_hooks.refreshWindowManager)
        m_hooks.refreshWindowManager();
    if (m_hooks.refreshSystem)
        m_hooks.refreshSystem();
    if (m_hooks.refreshAi)
        m_hooks.refreshAi();
}

void SessionLifecycle::evaluateResumeHealth(quint64 generation)
{
    if (generation != m_resumeGeneration)
        return;

    const State current = m_hooks.readState ? m_hooks.readState() : State{};
    if (requiredStateRecovered(current)) {
        writeHealthEvent(true, current);
        emit resumeHealthReported(true);
        return;
    }

    if (monotonicMilliseconds() - m_resumeStartedAtMs >= m_options.healthTimeoutMs) {
        writeHealthEvent(false, current);
        emit resumeHealthReported(false);
        return;
    }

    if (m_hooks.refreshSystem)
        m_hooks.refreshSystem();
    if (m_beforeSleep.aiConnected && !current.aiConnected && m_hooks.refreshAi)
        m_hooks.refreshAi();
    if (m_beforeSleep.compositorConnected && !current.compositorConnected
        && m_hooks.refreshWindowManager) {
        m_hooks.refreshWindowManager();
    }

    QTimer::singleShot(m_options.healthCheckIntervalMs, this, [this, generation] {
        evaluateResumeHealth(generation);
    });
}

bool SessionLifecycle::requiredStateRecovered(const State &current) const
{
    return (!m_beforeSleep.compositorConnected || current.compositorConnected)
        && (!m_beforeSleep.desktopProtocolAvailable || current.desktopProtocolAvailable)
        && (!m_beforeSleep.browserMapped || current.browserMapped)
        && (!m_beforeSleep.aiConnected || current.aiConnected)
        && (!m_beforeSleep.aiProviderAvailable || current.aiProviderAvailable)
        && (!m_beforeSleep.networkManagerAvailable || current.networkManagerAvailable)
        && (!m_beforeSleep.wifiAvailable || current.wifiAvailable)
        && (!m_beforeSleep.wifiEnabled || current.wifiEnabled)
        && (!m_beforeSleep.wifiConnected || current.wifiConnected)
        && (!m_beforeSleep.bluezServiceAvailable || current.bluezServiceAvailable)
        && (!m_beforeSleep.bluetoothAvailable || current.bluetoothAvailable)
        && (!m_beforeSleep.bluetoothPowered || current.bluetoothPowered)
        && (!m_beforeSleep.audioAvailable || current.audioAvailable)
        && (!m_beforeSleep.inputProtocolAvailable || current.inputProtocolAvailable)
        && (m_beforeSleep.touchpadCount == 0
            || current.touchpadCount >= m_beforeSleep.touchpadCount)
        && (!m_beforeSleep.batteryAvailable || current.batteryAvailable)
        && (!m_beforeSleep.brightnessAvailable || current.brightnessAvailable)
        && (!m_beforeSleep.powerModeAvailable || current.powerModeAvailable);
}

void SessionLifecycle::writePreparingEvent() const
{
    writeLiveEvent(QStringLiteral(
                       "MOKO_SLEEP state=preparing compositor=%1 browser_running=%2 ai=%3 "
                       "network_manager=%4 wifi_connected=%5 bluez_service=%6 "
                       "bluetooth_adapter=%7 bluetooth_powered=%8 audio=%9 "
                       "input_protocol=%10 touchpads=%11 battery=%12 power_mode=%13 uid=%14")
                       .arg(flag(m_beforeSleep.compositorConnected))
                       .arg(flag(m_beforeSleep.browserMapped))
                       .arg(flag(m_beforeSleep.aiConnected))
                       .arg(flag(m_beforeSleep.networkManagerAvailable))
                       .arg(flag(m_beforeSleep.wifiConnected))
                       .arg(flag(m_beforeSleep.bluezServiceAvailable))
                       .arg(flag(m_beforeSleep.bluetoothAvailable))
                       .arg(flag(m_beforeSleep.bluetoothPowered))
                       .arg(flag(m_beforeSleep.audioAvailable))
                       .arg(flag(m_beforeSleep.inputProtocolAvailable))
                       .arg(m_beforeSleep.touchpadCount)
                       .arg(flag(m_beforeSleep.batteryAvailable))
                       .arg(flag(m_beforeSleep.powerModeAvailable))
                       .arg(static_cast<qulonglong>(geteuid())));
}

void SessionLifecycle::writeResumedEvent() const
{
    writeLiveEvent(QStringLiteral("MOKO_SLEEP state=resumed uid=%1")
                       .arg(static_cast<qulonglong>(geteuid())));
}

void SessionLifecycle::writeHealthEvent(bool passed, const State &current) const
{
    writeLiveEvent(QStringLiteral(
                       "MOKO_RESUME_HEALTH result=%1 desktop_protocol=%2 compositor=%3 "
                       "browser_expected=%4 browser_mapped=%5 ai=%6 provider=%7 "
                       "network_manager=%8 wifi_device=%9 wifi_enabled=%10 wifi_connected=%11 "
                       "bluez_service=%12 bluetooth_adapter=%13 bluetooth_powered=%14 "
                       "audio=%15 input_protocol=%16 touchpads=%17 battery=%18 brightness=%19 "
                       "power_mode=%20 uid=%21")
                       .arg(passed ? QStringLiteral("pass") : QStringLiteral("fail"))
                       .arg(flag(current.desktopProtocolAvailable))
                       .arg(flag(current.compositorConnected))
                       .arg(flag(m_beforeSleep.browserMapped))
                       .arg(flag(current.browserMapped))
                       .arg(flag(current.aiConnected))
                       .arg(flag(current.aiProviderAvailable))
                       .arg(flag(current.networkManagerAvailable))
                       .arg(flag(current.wifiAvailable))
                       .arg(flag(current.wifiEnabled))
                       .arg(flag(current.wifiConnected))
                       .arg(flag(current.bluezServiceAvailable))
                       .arg(flag(current.bluetoothAvailable))
                       .arg(flag(current.bluetoothPowered))
                       .arg(flag(current.audioAvailable))
                       .arg(flag(current.inputProtocolAvailable))
                       .arg(current.touchpadCount)
                       .arg(flag(current.batteryAvailable))
                       .arg(flag(current.brightnessAvailable))
                       .arg(flag(current.powerModeAvailable))
                       .arg(static_cast<qulonglong>(geteuid())));
}
