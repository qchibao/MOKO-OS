#include "windowmanager.h"

#include "moko-window-control-v1-client-protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QtMath>

#include <cerrno>
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
        native->owner->m_protocolVersion = qMin(version, 2U);
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
};

void writeLiveInputEvent(const QString &message)
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
    if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        return;

    m_native->display = wl_display_connect(nullptr);
    if (m_native->display == nullptr)
        return;
    m_native->registry = wl_display_get_registry(m_native->display);
    wl_registry_add_listener(m_native->registry, &registryListener, m_native.get());
    if (wl_display_roundtrip(m_native->display) < 0 || m_native->manager == nullptr) {
        disconnectWayland();
        return;
    }

    moko_window_manager_v1_add_listener(m_native->manager, &managerListener, m_native.get());
    if (wl_display_roundtrip(m_native->display) < 0) {
        disconnectWayland();
        return;
    }

    m_connected = true;
    m_notifier = new QSocketNotifier(wl_display_get_fd(m_native->display),
                                     QSocketNotifier::Read,
                                     this);
    connect(m_notifier, &QSocketNotifier::activated, this, &WindowManager::dispatchWayland);
}

WindowManager::~WindowManager()
{
    disconnectWayland();
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

void WindowManager::reportInputPanelOpened() const
{
    writeLiveInputEvent(QStringLiteral(
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
    if (m_connected) {
        m_connected = false;
        m_windows.clear();
        m_protocolVersion = 0;
        m_touchpadCount = 0;
        m_inputCapabilities = 0;
        m_inputState = 0;
        m_pointerAcceleration = 0;
        ++m_revision;
        emit connectedChanged();
        emit windowsChanged();
        emit inputChanged();
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
