#include "windowmanager.h"

#include "moko-window-control-v1-client-protocol.h"

#include <QSocketNotifier>

#include <cerrno>
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
        native->manager = static_cast<moko_window_manager_v1 *>(
            wl_registry_bind(registry, name, &moko_window_manager_v1_interface,
                             qMin(version, 1U)));
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
};

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
        ++m_revision;
        emit connectedChanged();
        emit windowsChanged();
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
