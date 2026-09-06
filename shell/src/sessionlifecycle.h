#pragma once

#include <QObject>

#include <functional>

class SessionLifecycle final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool preparingForSleep READ preparingForSleep NOTIFY preparingForSleepChanged)

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
    };

    explicit SessionLifecycle(Hooks hooks, QObject *parent = nullptr);
    SessionLifecycle(Hooks hooks, Options options, QObject *parent = nullptr);

    bool preparingForSleep() const;

public slots:
    void handlePrepareForSleep(bool preparing);

signals:
    void preparingForSleepChanged();
    void resumed();
    void resumeHealthReported(bool passed);

private:
    void refreshAfterResume();
    void evaluateResumeHealth(quint64 generation);
    bool requiredStateRecovered(const State &current) const;
    void writePreparingEvent() const;
    void writeResumedEvent() const;
    void writeHealthEvent(bool passed, const State &current) const;

    Hooks m_hooks;
    Options m_options;
    State m_beforeSleep;
    bool m_preparingForSleep = false;
    quint64 m_resumeGeneration = 0;
    qint64 m_resumeStartedAtMs = 0;
};
