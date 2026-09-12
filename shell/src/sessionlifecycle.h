#pragma once

#include <QObject>

#include <functional>

class SessionLifecycle final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool preparingForSleep READ preparingForSleep NOTIFY preparingForSleepChanged)
    Q_PROPERTY(bool shuttingDown READ shuttingDown NOTIFY shuttingDownChanged)

public:
    struct State
    {
        bool compositorConnected = false;
        bool desktopProtocolAvailable = false;
        bool browserMapped = false;
        bool aiConnected = false;
        bool aiProviderAvailable = false;
        bool networkManagerAvailable = false;
        bool wifiAvailable = false;
        bool wifiEnabled = false;
        bool wifiConnected = false;
        bool bluezServiceAvailable = false;
        bool bluetoothAvailable = false;
        bool bluetoothPowered = false;
        bool audioAvailable = false;
        bool inputProtocolAvailable = false;
        int touchpadCount = 0;
        bool batteryAvailable = false;
        bool brightnessAvailable = false;
        bool powerModeAvailable = false;
    };

    struct Hooks
    {
        std::function<void()> refreshSystem;
        std::function<void()> refreshAi;
        std::function<bool()> refreshWindowManager;
        std::function<State()> readState;
    };

    struct Options
    {
        bool observeLogind = true;
        int healthTimeoutMs = 30000;
        int healthCheckIntervalMs = 1000;
        int shutdownFadeDelayMs = 0;
        // Keep the acknowledged black frame on scanout long enough for
        // slower DRM backends (and physical panels) to latch it before
        // logind is allowed to continue powering off.
        int shutdownReleaseGraceMs = 5000;
    };

    explicit SessionLifecycle(Hooks hooks, QObject *parent = nullptr);
    SessionLifecycle(Hooks hooks, Options options, QObject *parent = nullptr);
    ~SessionLifecycle() override;

    bool preparingForSleep() const;
    bool shuttingDown() const;

public slots:
    bool beginShutdown();
    bool cancelShutdown();
    bool notifyPowerActionRequested();
    void handlePrepareForSleep(bool preparing);
    void handlePrepareForShutdown(bool preparing);
    void notifyShutdownBlackoutPrepared();
    void notifyShutdownBlackoutPresented();

signals:
    void preparingForSleepChanged();
    void shuttingDownChanged();
    void shutdownBlackoutPrepared();
    void shutdownBlackoutReady();
    void shutdownFadeCompleted();
    void resumed();
    void resumeHealthReported(bool passed);

private:
    void refreshAfterResume();
    void evaluateResumeHealth(quint64 generation);
    bool requiredStateRecovered(const State &current) const;
    void writePreparingEvent() const;
    void writeResumedEvent() const;
    void writeHealthEvent(bool passed, const State &current) const;
    void writeShutdownEvent(const QString &state) const;
    void tryCompleteShutdownFade();
    void completeShutdownFade(quint64 generation);
    void scheduleShutdownInhibitorRelease();
    void resetShutdownState();
    void acquireShutdownInhibitor();
    void scheduleShutdownInhibitorRetry();
    void releaseShutdownInhibitor();

    Hooks m_hooks;
    Options m_options;
    State m_beforeSleep;
    bool m_preparingForSleep = false;
    bool m_shuttingDown = false;
    bool m_shutdownVisualReady = false;
    bool m_logindShutdownPreparing = false;
    bool m_shutdownCompletionScheduled = false;
    bool m_shutdownCompleted = false;
    bool m_shutdownActionRequested = false;
    bool m_shutdownReleaseScheduled = false;
    quint64 m_resumeGeneration = 0;
    quint64 m_shutdownGeneration = 0;
    quint64 m_shutdownPreparedGeneration = 0;
    quint64 m_shutdownPresentedGeneration = 0;
    qint64 m_resumeStartedAtMs = 0;
    int m_shutdownInhibitorFd = -1;
    bool m_shutdownInhibitorRetryScheduled = false;
};
