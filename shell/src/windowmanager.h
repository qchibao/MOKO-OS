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
    Q_PROPERTY(bool inputProtocolAvailable READ inputProtocolAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool touchpadAvailable READ touchpadAvailable NOTIFY inputChanged)
    Q_PROPERTY(int touchpadCount READ touchpadCount NOTIFY inputChanged)
    Q_PROPERTY(bool tapToClickAvailable READ tapToClickAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool tapToClickEnabled READ tapToClickEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool twoFingerScrollAvailable READ twoFingerScrollAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool twoFingerScrollEnabled READ twoFingerScrollEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool naturalScrollAvailable READ naturalScrollAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool naturalScrollEnabled READ naturalScrollEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool secondaryClickAvailable READ secondaryClickAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool secondaryClickEnabled READ secondaryClickEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool pointerAccelerationAvailable READ pointerAccelerationAvailable NOTIFY inputChanged)
    Q_PROPERTY(int pointerAcceleration READ pointerAcceleration NOTIFY inputChanged)
    Q_PROPERTY(bool palmRejectionManaged READ palmRejectionManaged NOTIFY inputChanged)
    Q_PROPERTY(bool disableWhileTypingAvailable READ disableWhileTypingAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool disableWhileTypingEnabled READ disableWhileTypingEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool dragAvailable READ dragAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool dragEnabled READ dragEnabled NOTIFY inputChanged)
    Q_PROPERTY(bool gesturesAvailable READ gesturesAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool desktopProtocolAvailable READ desktopProtocolAvailable NOTIFY desktopChanged)
    Q_PROPERTY(int outputScale READ outputScale NOTIFY desktopChanged)
    Q_PROPERTY(bool outputScale200Available READ outputScale200Available NOTIFY desktopChanged)
    Q_PROPERTY(int keyboardLayout READ keyboardLayout NOTIFY desktopChanged)
    Q_PROPERTY(QString keyboardLayoutName READ keyboardLayoutName NOTIFY desktopChanged)

public:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;

    bool connected() const;
    int revision() const;
    bool inputProtocolAvailable() const;
    bool touchpadAvailable() const;
    int touchpadCount() const;
    bool tapToClickAvailable() const;
    bool tapToClickEnabled() const;
    bool twoFingerScrollAvailable() const;
    bool twoFingerScrollEnabled() const;
    bool naturalScrollAvailable() const;
    bool naturalScrollEnabled() const;
    bool secondaryClickAvailable() const;
    bool secondaryClickEnabled() const;
    bool pointerAccelerationAvailable() const;
    int pointerAcceleration() const;
    bool palmRejectionManaged() const;
    bool disableWhileTypingAvailable() const;
    bool disableWhileTypingEnabled() const;
    bool dragAvailable() const;
    bool dragEnabled() const;
    bool gesturesAvailable() const;
    bool desktopProtocolAvailable() const;
    int outputScale() const;
    bool outputScale200Available() const;
    int keyboardLayout() const;
    QString keyboardLayoutName() const;

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
    Q_INVOKABLE bool setNaturalScrollEnabled(bool enabled);
    Q_INVOKABLE bool setPointerAcceleration(int speed);
    Q_INVOKABLE bool setOutputScale(int scalePercent);
    Q_INVOKABLE bool saveOutputScale(int scalePercent = -1);
    Q_INVOKABLE bool setKeyboardLayout(int layout);
    Q_INVOKABLE bool toggleKeyboardLayout();
    Q_INVOKABLE bool setShellOverlay(bool visible);
    bool prepareShutdown();
    Q_INVOKABLE bool refreshConnection();
    Q_INVOKABLE void reportInputPanelOpened() const;

signals:
    void connectedChanged();
    void windowsChanged();
    void brightnessStepRequested(int delta);
    void inputChanged();
    void desktopChanged();
    void globalActionRequested(const QString &action);
    void shutdownBlackoutPresented();

private:
    friend struct WindowManagerCallbacks;

    struct NativeState;
    struct WindowInfo;

    void dispatchWayland();
    bool connectWayland();
    void disconnectWayland();
    void applySavedDesktopSettings();
    void updateWindow(quint32 id, const QString &appId, const QString &title, quint32 state);
    void removeWindow(quint32 id);
    void completeUpdate();
    void updateInputConfig(quint32 deviceCount,
                           quint32 capabilities,
                           quint32 state,
                           qint32 pointerAcceleration);
    void updateDesktopConfig(quint32 outputScale,
                             quint32 scaleCapabilities,
                             quint32 keyboardLayout);
    void handleShutdownBlackoutPresented();
    void pumpShutdownEvents(quint64 generation);
    bool flushRequest();
    const WindowInfo *windowForApplication(const QString &appId) const;
    bool sendRequest(const QString &appId, const std::function<void(quint32)> &request);

    std::unique_ptr<NativeState> m_native;
    QList<WindowInfo> m_windows;
    QSocketNotifier *m_notifier = nullptr;
    int m_revision = 0;
    bool m_connected = false;
    quint32 m_protocolVersion = 0;
    quint32 m_touchpadCount = 0;
    quint32 m_inputCapabilities = 0;
    quint32 m_inputState = 0;
    int m_pointerAcceleration = 0;
    int m_outputScale = 100;
    quint32 m_outputScaleCapabilities = 1;
    int m_keyboardLayout = 0;
    quint64 m_shutdownRequestGeneration = 0;
    int m_shutdownEventPumpAttempts = 0;
    bool m_shutdownBlackoutPending = false;
};
