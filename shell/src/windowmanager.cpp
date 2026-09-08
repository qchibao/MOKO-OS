#include "windowmanager.h"

#include "moko-window-control-v1-client-protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSocketNotifier>
#include <QtMath>

#include <cerrno>
#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>

struct WindowManager::WindowInfo
{
    quint32 id = 0;
    QString appId;
    QString title;
    quint32 state = 0;
};

struct WindowManager::NativeState
{
    WindowManager *owner = nullptr;
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    moko_window_manager_v1 *manager = nullptr;
};

struct WindowManagerCallbacks
{
    static void registryGlobal(void *data,
                               wl_registry *registry,
                               uint32_t name,
                               const char *interface,
                               uint32_t version)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        if (qstrcmp(interface, moko_window_manager_v1_interface.name) != 0)
            return;
        native->owner->m_protocolVersion = qMin(version, 5U);
        native->manager = static_cast<moko_window_manager_v1 *>(
            wl_registry_bind(registry, name, &moko_window_manager_v1_interface,
                             native->owner->m_protocolVersion));
    }

    static void registryGlobalRemove(void *, wl_registry *, uint32_t)
    {
    }

    static void managerWindow(void *data,
                              moko_window_manager_v1 *,
                              uint32_t id,
                              const char *appId,
                              const char *title,
                              uint32_t state)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->updateWindow(id,
                                    QString::fromUtf8(appId),
                                    QString::fromUtf8(title),
                                    state);
    }

    static void managerWindowRemoved(void *data, moko_window_manager_v1 *, uint32_t id)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->removeWindow(id);
    }

    static void managerDone(void *data, moko_window_manager_v1 *)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->completeUpdate();
    }

    static void managerBrightnessStep(void *data, moko_window_manager_v1 *, int32_t delta)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        emit native->owner->brightnessStepRequested(delta);
    }

    static void managerInputConfig(void *data,
                                   moko_window_manager_v1 *,
                                   uint32_t deviceCount,
                                   uint32_t capabilities,
                                   uint32_t state,
                                   int32_t pointerAcceleration)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->updateInputConfig(deviceCount,
                                         capabilities,
                                         state,
                                         pointerAcceleration);
    }

    static void managerDesktopConfig(void *data,
                                     moko_window_manager_v1 *,
                                     uint32_t outputScale,
                                     uint32_t scaleCapabilities,
                                     uint32_t keyboardLayout)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->updateDesktopConfig(outputScale, scaleCapabilities, keyboardLayout);
    }

    static void managerGestureConfig(void *data,
                                     moko_window_manager_v1 *,
                                     uint32_t capabilities,
                                     uint32_t state)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->updateGestureConfig(capabilities, state);
    }

    static void managerGlobalAction(void *data,
                                    moko_window_manager_v1 *,
                                    uint32_t action)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        QString name;
        switch (action) {
        case MOKO_WINDOW_MANAGER_V1_GLOBAL_ACTION_LAUNCHER:
            name = QStringLiteral("launcher");
            break;
        case MOKO_WINDOW_MANAGER_V1_GLOBAL_ACTION_AI:
            name = QStringLiteral("ai");
            break;
        case MOKO_WINDOW_MANAGER_V1_GLOBAL_ACTION_NOTIFICATION_CENTER:
            name = QStringLiteral("notification-center");
            break;
        case MOKO_WINDOW_MANAGER_V1_GLOBAL_ACTION_SCREENSHOT:
            name = QStringLiteral("screenshot");
            break;
        default:
            return;
        }
        emit native->owner->globalActionRequested(name);
    }

    static void managerShutdownBlackoutPresented(void *data, moko_window_manager_v1 *)
    {
        auto *native = static_cast<WindowManager::NativeState *>(data);
        native->owner->handleShutdownBlackoutPresented();
    }
};

namespace {

const wl_registry_listener registryListener = {
    .global = WindowManagerCallbacks::registryGlobal,
    .global_remove = WindowManagerCallbacks::registryGlobalRemove,
};

const moko_window_manager_v1_listener managerListener = {
    .window = WindowManagerCallbacks::managerWindow,
    .window_removed = WindowManagerCallbacks::managerWindowRemoved,
    .done = WindowManagerCallbacks::managerDone,
    .brightness_step = WindowManagerCallbacks::managerBrightnessStep,
    .input_config = WindowManagerCallbacks::managerInputConfig,
    .desktop_config = WindowManagerCallbacks::managerDesktopConfig,
    .global_action = WindowManagerCallbacks::managerGlobalAction,
    .shutdown_blackout_presented = WindowManagerCallbacks::managerShutdownBlackoutPresented,
    .gesture_config = WindowManagerCallbacks::managerGestureConfig,
};

constexpr int shutdownEventPumpIntervalMs = 20;
constexpr int shutdownEventPumpAttemptLimit = 200;

void writeLiveEvent(const QString &message)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(message.toUtf8());
    file.write("\n");
}

} // namespace

WindowManager::WindowManager(QObject *parent)
    : QObject(parent)
    , m_native(std::make_unique<NativeState>())
{
    m_native->owner = this;
    connectWayland();
}

WindowManager::~WindowManager()
{
    disconnectWayland();
}

bool WindowManager::connectWayland()
{
    if (m_connected)
        return true;
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        return false;

    m_native->display = wl_display_connect(nullptr);
    if (m_native->display == nullptr)
        return false;
    m_native->registry = wl_display_get_registry(m_native->display);
    wl_registry_add_listener(m_native->registry, &registryListener, m_native.get());
    if (wl_display_roundtrip(m_native->display) < 0 || m_native->manager == nullptr) {
        disconnectWayland();
        return false;
    }

    moko_window_manager_v1_add_listener(m_native->manager, &managerListener, m_native.get());
    if (wl_display_roundtrip(m_native->display) < 0) {
        disconnectWayland();
        return false;
    }

    m_connected = true;
    writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER state=connected protocol=%1 manager=%2 fd=%3 uid=%4")
                       .arg(m_protocolVersion)
                       .arg(m_native->manager != nullptr ? 1 : 0)
                       .arg(wl_display_get_fd(m_native->display))
                       .arg(static_cast<qulonglong>(geteuid())));
    m_notifier = new QSocketNotifier(wl_display_get_fd(m_native->display),
                                     QSocketNotifier::Read,
                                     this);
    connect(m_notifier, &QSocketNotifier::activated, this, &WindowManager::dispatchWayland);
    emit connectedChanged();
    applySavedDesktopSettings();
    return true;
}

bool WindowManager::connected() const
{
    return m_connected;
}

int WindowManager::revision() const
{
    return m_revision;
}

bool WindowManager::inputProtocolAvailable() const { return m_protocolVersion >= 2; }
bool WindowManager::touchpadAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TRACKPAD) != 0;
}
int WindowManager::touchpadCount() const { return static_cast<int>(m_touchpadCount); }
bool WindowManager::tapToClickAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TAP) != 0;
}
bool WindowManager::tapToClickEnabled() const
{
    return (m_inputState & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TAP_ENABLED) != 0;
}
bool WindowManager::twoFingerScrollAvailable() const
{
    return (m_inputCapabilities
            & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TWO_FINGER_SCROLL) != 0;
}
bool WindowManager::twoFingerScrollEnabled() const
{
    return (m_inputState
            & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TWO_FINGER_SCROLL_ENABLED) != 0;
}
bool WindowManager::naturalScrollAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_NATURAL_SCROLL) != 0;
}
bool WindowManager::naturalScrollEnabled() const
{
    return (m_inputState & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_NATURAL_SCROLL_ENABLED) != 0;
}
bool WindowManager::secondaryClickAvailable() const
{
    return (m_inputCapabilities
            & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_SECONDARY_CLICK) != 0;
}
bool WindowManager::secondaryClickEnabled() const
{
    return (m_inputState & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_SECONDARY_CLICK_ENABLED) != 0;
}
bool WindowManager::pointerAccelerationAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_ACCELERATION) != 0;
}
int WindowManager::pointerAcceleration() const { return m_pointerAcceleration; }
bool WindowManager::palmRejectionManaged() const { return touchpadAvailable(); }
bool WindowManager::disableWhileTypingAvailable() const
{
    return (m_inputCapabilities
            & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DISABLE_WHILE_TYPING) != 0;
}
bool WindowManager::disableWhileTypingEnabled() const
{
    return (m_inputState
            & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DISABLE_WHILE_TYPING_ENABLED) != 0;
}
bool WindowManager::dragAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DRAG) != 0;
}
bool WindowManager::dragEnabled() const
{
    return (m_inputState & MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DRAG_ENABLED) != 0;
}
bool WindowManager::gesturesAvailable() const
{
    return (m_inputCapabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_GESTURES) != 0;
}
bool WindowManager::gestureProtocolAvailable() const { return m_protocolVersion >= 5; }
bool WindowManager::threeFingerDragAvailable() const
{
    return (m_gestureCapabilities
            & MOKO_WINDOW_MANAGER_V1_GESTURE_CAPABILITY_THREE_FINGER_DRAG) != 0;
}
bool WindowManager::threeFingerDragEnabled() const
{
    return (m_gestureState
            & MOKO_WINDOW_MANAGER_V1_GESTURE_STATE_THREE_FINGER_DRAG_ENABLED) != 0;
}
bool WindowManager::browserHistorySwipeAvailable() const
{
    return (m_gestureCapabilities
            & MOKO_WINDOW_MANAGER_V1_GESTURE_CAPABILITY_BROWSER_HISTORY_SWIPE) != 0;
}
bool WindowManager::browserHistorySwipeEnabled() const
{
    return (m_gestureState
            & MOKO_WINDOW_MANAGER_V1_GESTURE_STATE_BROWSER_HISTORY_SWIPE_ENABLED) != 0;
}

bool WindowManager::desktopProtocolAvailable() const { return m_protocolVersion >= 3; }
int WindowManager::outputScale() const { return m_outputScale; }
bool WindowManager::outputScale200Available() const
{
    return (m_outputScaleCapabilities
            & MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_200) != 0;
}
int WindowManager::keyboardLayout() const { return m_keyboardLayout; }
QString WindowManager::keyboardLayoutName() const
{
    return m_keyboardLayout == MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE
        ? QStringLiteral("Vietnamese") : QStringLiteral("English");
}

bool WindowManager::isRunning(const QString &appId) const
{
    return windowForApplication(appId) != nullptr;
}

bool WindowManager::isActive(const QString &appId) const
{
    const WindowInfo *window = windowForApplication(appId);
    return window != nullptr
        && (window->state & MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED) != 0;
}

bool WindowManager::isMinimized(const QString &appId) const
{
    const WindowInfo *window = windowForApplication(appId);
    return window != nullptr
        && (window->state & MOKO_WINDOW_MANAGER_V1_STATE_MINIMIZED) != 0;
}

int WindowManager::windowCount(const QString &appId) const
{
    int count = 0;
    for (const WindowInfo &window : m_windows)
        count += window.appId == appId ? 1 : 0;
    return count;
}

bool WindowManager::activateApplication(const QString &appId)
{
    return sendRequest(appId, [this](quint32 id) {
        moko_window_manager_v1_activate(m_native->manager, id);
    });
}

bool WindowManager::minimizeApplication(const QString &appId)
{
    return sendRequest(appId, [this](quint32 id) {
        moko_window_manager_v1_set_minimized(m_native->manager, id, 1);
    });
}

bool WindowManager::toggleMaximizeApplication(const QString &appId)
{
    const WindowInfo *window = windowForApplication(appId);
    if (window == nullptr)
        return false;
    const bool maximized = window->state & MOKO_WINDOW_MANAGER_V1_STATE_MAXIMIZED;
    return sendRequest(appId, [this, maximized](quint32 id) {
        moko_window_manager_v1_set_maximized(m_native->manager, id, maximized ? 0 : 1);
    });
}

bool WindowManager::toggleFullscreenApplication(const QString &appId)
{
    const WindowInfo *window = windowForApplication(appId);
    if (window == nullptr)
        return false;
    const bool fullscreen = window->state & MOKO_WINDOW_MANAGER_V1_STATE_FULLSCREEN;
    return sendRequest(appId, [this, fullscreen](quint32 id) {
        moko_window_manager_v1_set_fullscreen(m_native->manager, id, fullscreen ? 0 : 1);
    });
}

bool WindowManager::snapApplication(const QString &appId, const QString &side)
{
    uint32_t protocolSide = MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_NONE;
    if (side.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0)
        protocolSide = MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_LEFT;
    else if (side.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0)
        protocolSide = MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_RIGHT;
    return sendRequest(appId, [this, protocolSide](quint32 id) {
        moko_window_manager_v1_snap(m_native->manager, id, protocolSide);
    });
}

bool WindowManager::closeApplication(const QString &appId)
{
    return sendRequest(appId, [this](quint32 id) {
        moko_window_manager_v1_close(m_native->manager, id);
    });
}

bool WindowManager::setNaturalScrollEnabled(bool enabled)
{
    if (!inputProtocolAvailable() || !naturalScrollAvailable()
        || m_native->manager == nullptr) {
        return false;
    }
    moko_window_manager_v1_set_natural_scroll(m_native->manager, enabled ? 1 : 0);
    if (wl_display_flush(m_native->display) < 0 && errno != EAGAIN) {
        disconnectWayland();
        return false;
    }
    return true;
}

bool WindowManager::setPointerAcceleration(int speed)
{
    if (!inputProtocolAvailable() || !pointerAccelerationAvailable()
        || m_native->manager == nullptr) {
        return false;
    }
    const int boundedSpeed = qBound(-100, speed, 100);
    moko_window_manager_v1_set_pointer_acceleration(m_native->manager, boundedSpeed * 10);
    if (wl_display_flush(m_native->display) < 0 && errno != EAGAIN) {
        disconnectWayland();
        return false;
    }
    return true;
}

bool WindowManager::setThreeFingerDragEnabled(bool enabled)
{
    if (!gestureProtocolAvailable() || !threeFingerDragAvailable()
        || m_native->manager == nullptr)
        return false;
    moko_window_manager_v1_set_gesture_enabled(
        m_native->manager, MOKO_WINDOW_MANAGER_V1_GESTURE_THREE_FINGER_DRAG,
        enabled ? 1 : 0);
    if (!flushRequest())
        return false;
    QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"))
        .setValue(QStringLiteral("input/threeFingerDrag"), enabled);
    return true;
}

bool WindowManager::setBrowserHistorySwipeEnabled(bool enabled)
{
    if (!gestureProtocolAvailable() || !browserHistorySwipeAvailable()
        || m_native->manager == nullptr)
        return false;
    moko_window_manager_v1_set_gesture_enabled(
        m_native->manager, MOKO_WINDOW_MANAGER_V1_GESTURE_BROWSER_HISTORY_SWIPE,
        enabled ? 1 : 0);
    if (!flushRequest())
        return false;
    QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"))
        .setValue(QStringLiteral("input/browserHistorySwipe"), enabled);
    return true;
}

bool WindowManager::setOutputScale(int scalePercent)
{
    if (!desktopProtocolAvailable() || (scalePercent != 100 && scalePercent != 200)
        || (scalePercent == 200 && !outputScale200Available())
        || m_native->manager == nullptr) {
        return false;
    }
    moko_window_manager_v1_set_output_scale(m_native->manager,
                                            static_cast<uint32_t>(scalePercent));
    if (!flushRequest())
        return false;
    return true;
}

bool WindowManager::saveOutputScale(int scalePercent)
{
    const int value = scalePercent < 0 ? m_outputScale : scalePercent;
    if (value != 100 && value != 200)
        return false;
    if (value == 200 && !outputScale200Available())
        return false;
    QSettings().setValue(QStringLiteral("desktop/outputScale"), value);
    return true;
}

bool WindowManager::setKeyboardLayout(int layout)
{
    if (!desktopProtocolAvailable()
        || (layout != MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH
            && layout != MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE)
        || m_native->manager == nullptr) {
        return false;
    }
    moko_window_manager_v1_set_keyboard_layout(m_native->manager,
                                               static_cast<uint32_t>(layout));
    if (!flushRequest())
        return false;
    QSettings().setValue(QStringLiteral("desktop/keyboardLayout"), layout);
    return true;
}

bool WindowManager::toggleKeyboardLayout()
{
    return setKeyboardLayout(m_keyboardLayout
                                 == MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH
                             ? MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE
                             : MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH);
}

bool WindowManager::setShellOverlay(bool visible)
{
    if (!desktopProtocolAvailable() || m_native->manager == nullptr)
        return false;
    moko_window_manager_v1_set_shell_overlay(m_native->manager, visible ? 1 : 0);
    return flushRequest();
}

bool WindowManager::prepareShutdown()
{
    writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER action=prepare-shutdown state=requested protocol=%1 connected=%2 manager=%3 uid=%4")
                       .arg(m_protocolVersion)
                       .arg(m_connected ? 1 : 0)
                       .arg(m_native->manager != nullptr ? 1 : 0)
                       .arg(static_cast<qulonglong>(geteuid())));
    if (m_protocolVersion < 4 || m_native->manager == nullptr) {
        writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER action=prepare-shutdown state=unavailable protocol=%1 connected=%2 manager=%3 uid=%4")
                           .arg(m_protocolVersion)
                           .arg(m_connected ? 1 : 0)
                           .arg(m_native->manager != nullptr ? 1 : 0)
                           .arg(static_cast<qulonglong>(geteuid())));
        return false;
    }

    const quint64 generation = ++m_shutdownRequestGeneration;
    m_shutdownEventPumpAttempts = 0;
    m_shutdownBlackoutPending = true;
    moko_window_manager_v1_prepare_shutdown(m_native->manager);
    const bool flushed = flushRequest();
    writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER action=prepare-shutdown state=sent flushed=%1 generation=%2 uid=%3")
                       .arg(flushed ? 1 : 0)
                       .arg(static_cast<qulonglong>(generation))
                       .arg(static_cast<qulonglong>(geteuid())));
    if (!flushed) {
        m_shutdownBlackoutPending = false;
        return false;
    }

    // The custom protocol uses a separate Wayland connection from Qt's QPA.
    // Dispatch it explicitly during logind's bounded shutdown window so the
    // blackout acknowledgement does not depend on notifier scheduling.
    pumpShutdownEvents(generation);
    return true;
}

bool WindowManager::refreshConnection()
{
    disconnectWayland();
    return connectWayland();
}

void WindowManager::reportInputPanelOpened() const
{
    writeLiveEvent(QStringLiteral(
                            "MOKO_INPUT_PANEL state=open protocol=%1 touchpads=%2 capabilities=%3 "
                            "input_state=%4 acceleration=%5 uid=%6")
                            .arg(inputProtocolAvailable() ? 1 : 0)
                            .arg(m_touchpadCount)
                            .arg(m_inputCapabilities)
                            .arg(m_inputState)
                            .arg(m_pointerAcceleration)
                            .arg(static_cast<qulonglong>(geteuid())));
}

void WindowManager::dispatchWayland()
{
    if (m_native->display == nullptr)
        return;
    if (wl_display_dispatch(m_native->display) < 0)
        disconnectWayland();
}

void WindowManager::disconnectWayland()
{
    if (m_notifier != nullptr) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_native == nullptr)
        return;
    if (m_native->manager != nullptr) {
        moko_window_manager_v1_destroy(m_native->manager);
        m_native->manager = nullptr;
    }
    if (m_native->registry != nullptr) {
        wl_registry_destroy(m_native->registry);
        m_native->registry = nullptr;
    }
    if (m_native->display != nullptr) {
        wl_display_disconnect(m_native->display);
        m_native->display = nullptr;
    }
    const bool wasConnected = m_connected;
    const bool hadWindows = !m_windows.isEmpty();
    const bool hadInputState = m_protocolVersion >= 2 || m_touchpadCount != 0
        || m_inputCapabilities != 0 || m_inputState != 0 || m_pointerAcceleration != 0;
    const bool hadDesktopState = m_protocolVersion >= 3 || m_outputScale != 100
        || m_outputScaleCapabilities
            != MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_100
        || m_keyboardLayout != 0;
    m_connected = false;
    m_windows.clear();
    m_protocolVersion = 0;
    m_touchpadCount = 0;
    m_inputCapabilities = 0;
    m_inputState = 0;
    m_gestureCapabilities = 0;
    m_gestureState = 0;
    m_pointerAcceleration = 0;
    m_outputScale = 100;
    m_outputScaleCapabilities = MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_100;
    m_keyboardLayout = 0;
    if (wasConnected || hadWindows || hadInputState || hadDesktopState) {
        ++m_revision;
        if (wasConnected)
            emit connectedChanged();
        if (hadWindows)
            emit windowsChanged();
        if (hadInputState)
            emit inputChanged();
        if (hadDesktopState)
            emit desktopChanged();
    }
}

void WindowManager::applySavedDesktopSettings()
{
    if (!desktopProtocolAvailable())
        return;
    QSettings settings;
    const int savedScale = settings.value(QStringLiteral("desktop/outputScale"),
                                          m_outputScale).toInt();
    const int savedLayout = settings.value(QStringLiteral("desktop/keyboardLayout"),
                                           m_keyboardLayout).toInt();
    if (savedScale != m_outputScale)
        setOutputScale(savedScale);
    if (savedLayout != m_keyboardLayout)
        setKeyboardLayout(savedLayout);
    if (gestureProtocolAvailable()) {
        const QSettings shared(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"));
        const bool threeFinger = shared.value(
            QStringLiteral("input/threeFingerDrag"), true).toBool();
        const bool browserSwipe = shared.value(
            QStringLiteral("input/browserHistorySwipe"), true).toBool();
        if (threeFingerDragAvailable() && threeFinger != threeFingerDragEnabled())
            setThreeFingerDragEnabled(threeFinger);
        if (browserHistorySwipeAvailable()
            && browserSwipe != browserHistorySwipeEnabled())
            setBrowserHistorySwipeEnabled(browserSwipe);
    }
}

void WindowManager::updateWindow(quint32 id,
                                 const QString &appId,
                                 const QString &title,
                                 quint32 state)
{
    for (WindowInfo &window : m_windows) {
        if (window.id != id)
            continue;
        window.appId = appId;
        window.title = title;
        window.state = state;
        return;
    }
    m_windows.append(WindowInfo{id, appId, title, state});
}

void WindowManager::removeWindow(quint32 id)
{
    m_windows.removeIf([id](const WindowInfo &window) { return window.id == id; });
}

void WindowManager::completeUpdate()
{
    ++m_revision;
    emit windowsChanged();
}

void WindowManager::updateInputConfig(quint32 deviceCount,
                                      quint32 capabilities,
                                      quint32 state,
                                      qint32 pointerAcceleration)
{
    const int speed = qBound(-100, qRound(pointerAcceleration / 10.0), 100);
    if (m_touchpadCount == deviceCount
        && m_inputCapabilities == capabilities
        && m_inputState == state
        && m_pointerAcceleration == speed) {
        return;
    }
    m_touchpadCount = deviceCount;
    m_inputCapabilities = capabilities;
    m_inputState = state;
    m_pointerAcceleration = speed;
    emit inputChanged();
}

void WindowManager::updateGestureConfig(quint32 capabilities, quint32 state)
{
    const quint32 capabilityMask =
        MOKO_WINDOW_MANAGER_V1_GESTURE_CAPABILITY_THREE_FINGER_DRAG
        | MOKO_WINDOW_MANAGER_V1_GESTURE_CAPABILITY_BROWSER_HISTORY_SWIPE;
    const quint32 stateMask =
        MOKO_WINDOW_MANAGER_V1_GESTURE_STATE_THREE_FINGER_DRAG_ENABLED
        | MOKO_WINDOW_MANAGER_V1_GESTURE_STATE_BROWSER_HISTORY_SWIPE_ENABLED;
    capabilities &= capabilityMask;
    state &= stateMask;
    if (m_gestureCapabilities == capabilities && m_gestureState == state)
        return;
    m_gestureCapabilities = capabilities;
    m_gestureState = state;
    emit inputChanged();
}

void WindowManager::updateDesktopConfig(quint32 outputScale,
                                        quint32 scaleCapabilities,
                                        quint32 keyboardLayout)
{
    const int scale = outputScale == 200 ? 200 : 100;
    const int layout = keyboardLayout
            == MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE
        ? MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE
        : MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH;
    const quint32 capabilities = scaleCapabilities
        & (MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_100
           | MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_200);
    if (m_outputScale == scale && m_outputScaleCapabilities == capabilities
        && m_keyboardLayout == layout) {
        return;
    }
    m_outputScale = scale;
    m_outputScaleCapabilities = capabilities;
    m_keyboardLayout = layout;
    emit desktopChanged();
}

void WindowManager::handleShutdownBlackoutPresented()
{
    writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER action=shutdown-ack state=received pending=%1 uid=%2")
                       .arg(m_shutdownBlackoutPending ? 1 : 0)
                       .arg(static_cast<qulonglong>(geteuid())));
    if (!m_shutdownBlackoutPending)
        return;
    m_shutdownBlackoutPending = false;
    emit shutdownBlackoutPresented();
}

void WindowManager::pumpShutdownEvents(quint64 generation)
{
    while (m_shutdownBlackoutPending
           && generation == m_shutdownRequestGeneration
           && m_native->display != nullptr
           && m_shutdownEventPumpAttempts < shutdownEventPumpAttemptLimit) {
        if (wl_display_dispatch_pending(m_native->display) < 0) {
            disconnectWayland();
            return;
        }
        if (!m_shutdownBlackoutPending)
            return;

        if (wl_display_flush(m_native->display) < 0 && errno != EAGAIN) {
            disconnectWayland();
            return;
        }

        pollfd descriptor = {
            .fd = wl_display_get_fd(m_native->display),
            .events = POLLIN,
            .revents = 0,
        };
        const int ready = ::poll(&descriptor, 1, shutdownEventPumpIntervalMs);
        ++m_shutdownEventPumpAttempts;
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            disconnectWayland();
            return;
        }
        if (ready == 0)
            continue;
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            disconnectWayland();
            return;
        }
        if ((descriptor.revents & POLLIN) != 0
            && wl_display_dispatch(m_native->display) < 0) {
            disconnectWayland();
            return;
        }
    }

    if (m_shutdownBlackoutPending) {
        writeLiveEvent(QStringLiteral("MOKO_WINDOW_MANAGER action=shutdown-ack state=timeout attempts=%1 uid=%2")
                           .arg(m_shutdownEventPumpAttempts)
                           .arg(static_cast<qulonglong>(geteuid())));
        m_shutdownBlackoutPending = false;
    }
}

bool WindowManager::flushRequest()
{
    if (wl_display_flush(m_native->display) < 0 && errno != EAGAIN) {
        disconnectWayland();
        return false;
    }
    return true;
}

const WindowManager::WindowInfo *WindowManager::windowForApplication(const QString &appId) const
{
    const WindowInfo *fallback = nullptr;
    for (const WindowInfo &window : m_windows) {
        if (window.appId != appId)
            continue;
        if ((window.state & MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED) != 0)
            return &window;
        if (fallback == nullptr)
            fallback = &window;
    }
    return fallback;
}

bool WindowManager::sendRequest(const QString &appId,
                                const std::function<void(quint32)> &request)
{
    if (!m_connected || m_native->manager == nullptr)
        return false;
    const WindowInfo *window = windowForApplication(appId);
    if (window == nullptr)
        return false;
    request(window->id);
    if (wl_display_flush(m_native->display) < 0 && errno != EAGAIN) {
        disconnectWayland();
        return false;
    }
    return true;
}
