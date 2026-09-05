#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include <functional>
#include <memory>

class QSocketNotifier;

class WindowManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int revision READ revision NOTIFY windowsChanged)

public:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;

    bool connected() const;
    int revision() const;

    Q_INVOKABLE bool isRunning(const QString &appId) const;
    Q_INVOKABLE bool isActive(const QString &appId) const;
    Q_INVOKABLE bool isMinimized(const QString &appId) const;
    Q_INVOKABLE int windowCount(const QString &appId) const;
    Q_INVOKABLE bool activateApplication(const QString &appId);
    Q_INVOKABLE bool minimizeApplication(const QString &appId);
    Q_INVOKABLE bool toggleMaximizeApplication(const QString &appId);
    Q_INVOKABLE bool toggleFullscreenApplication(const QString &appId);
    Q_INVOKABLE bool snapApplication(const QString &appId, const QString &side);
    Q_INVOKABLE bool closeApplication(const QString &appId);

signals:
    void connectedChanged();
    void windowsChanged();
    void brightnessStepRequested(int delta);

private:
    friend struct WindowManagerCallbacks;

    struct NativeState;
    struct WindowInfo;

    void dispatchWayland();
    void disconnectWayland();
    void updateWindow(quint32 id, const QString &appId, const QString &title, quint32 state);
    void removeWindow(quint32 id);
    void completeUpdate();
    const WindowInfo *windowForApplication(const QString &appId) const;
    bool sendRequest(const QString &appId, const std::function<void(quint32)> &request);

    std::unique_ptr<NativeState> m_native;
    QList<WindowInfo> m_windows;
    QSocketNotifier *m_notifier = nullptr;
    int m_revision = 0;
    bool m_connected = false;
};
