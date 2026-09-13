#include "windowmanager.h"

#include "moko-window-control-v1-server-protocol.h"

#include <QFile>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#include <wayland-server-core.h>

namespace {

class EnvironmentGuard
{
public:
    explicit EnvironmentGuard(const char *name)
        : m_name(name)
        , m_value(qgetenv(name))
        , m_wasSet(qEnvironmentVariableIsSet(name))
    {
    }

    ~EnvironmentGuard()
    {
        if (m_wasSet)
            qputenv(m_name, m_value);
        else
            qunsetenv(m_name);
    }

private:
    const char *m_name;
    QByteArray m_value;
    bool m_wasSet = false;
};

class FakeWaylandServer
{
public:
    ~FakeWaylandServer()
    {
        stop();
    }

    bool start(const QByteArray &socketName)
    {
        if (::pipe(m_controlPipe) != 0)
            return false;
        for (const int fd : m_controlPipe) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
                return false;
        }

        m_display = wl_display_create();
        if (m_display == nullptr)
            return false;
        wl_event_loop *loop = wl_display_get_event_loop(m_display);
        m_controlSource = wl_event_loop_add_fd(
            loop, m_controlPipe[0], WL_EVENT_READABLE, controlReady, this);
        if (m_controlSource == nullptr)
            return false;
        m_global = wl_global_create(m_display,
                                    &moko_window_manager_v1_interface,
                                    7,
                                    this,
                                    bindManager);
        if (m_global == nullptr || wl_display_add_socket(m_display, socketName.constData()) != 0)
            return false;

        m_thread = std::thread([this] { wl_display_run(m_display); });
        return true;
    }

    bool sendPowerMenu()
    {
        const char command = 'p';
        return ::write(m_controlPipe[1], &command, sizeof(command)) == sizeof(command);
    }

private:
    static void destroyManager(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void ignoreWindow(wl_client *, wl_resource *, uint32_t) {}
    static void ignoreWindowState(wl_client *, wl_resource *, uint32_t, uint32_t) {}
    static void ignoreInt(wl_client *, wl_resource *, int32_t) {}
    static void ignoreUint(wl_client *, wl_resource *, uint32_t) {}
    static void ignorePair(wl_client *, wl_resource *, uint32_t, uint32_t) {}
    static void ignoreRequest(wl_client *, wl_resource *) {}

    static const struct moko_window_manager_v1_interface *implementation()
    {
        static const struct moko_window_manager_v1_interface value = {
            .destroy = destroyManager,
            .activate = ignoreWindow,
            .set_minimized = ignoreWindowState,
            .set_maximized = ignoreWindowState,
            .set_fullscreen = ignoreWindowState,
            .snap = ignoreWindowState,
            .close = ignoreWindow,
            .set_natural_scroll = ignoreUint,
            .set_pointer_acceleration = ignoreInt,
            .set_output_scale = ignoreUint,
            .set_keyboard_layout = ignoreUint,
            .set_shell_overlay = ignoreUint,
            .prepare_shutdown = ignoreRequest,
            .set_gesture_enabled = ignorePair,
            .set_power_key_handling = ignoreUint,
            .present_shell_overlay = ignoreUint,
        };
        return &value;
    }

    static void resourceDestroyed(wl_resource *resource)
    {
        auto *server = static_cast<FakeWaylandServer *>(wl_resource_get_user_data(resource));
        if (server->m_managerResource == resource)
            server->m_managerResource = nullptr;
    }

    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *server = static_cast<FakeWaylandServer *>(data);
        server->m_managerResource = wl_resource_create(
            client, &moko_window_manager_v1_interface, std::min(version, 7U), id);
        if (server->m_managerResource == nullptr) {
            wl_client_post_no_memory(client);
            return;
        }
        wl_resource_set_implementation(server->m_managerResource,
                                       implementation(),
                                       server,
                                       resourceDestroyed);
    }

    static int controlReady(int fd, uint32_t mask, void *data)
    {
        auto *server = static_cast<FakeWaylandServer *>(data);
        if ((mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) != 0) {
            wl_display_terminate(server->m_display);
            return 0;
        }

        char commands[16];
        ssize_t count = 0;
        while ((count = ::read(fd, commands, sizeof(commands))) > 0) {
            for (ssize_t index = 0; index < count; ++index) {
                if (commands[index] == 'q') {
                    wl_display_terminate(server->m_display);
                } else if (commands[index] == 'p' && server->m_managerResource != nullptr) {
                    moko_window_manager_v1_send_power_menu(server->m_managerResource);
                    wl_client_flush(wl_resource_get_client(server->m_managerResource));
                }
            }
        }
        if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            wl_display_terminate(server->m_display);
        return 0;
    }

    void stop()
    {
        if (m_thread.joinable()) {
            const char command = 'q';
            (void)::write(m_controlPipe[1], &command, sizeof(command));
            m_thread.join();
        }
        if (m_global != nullptr) {
            wl_global_destroy(m_global);
            m_global = nullptr;
        }
        if (m_controlSource != nullptr) {
            wl_event_source_remove(m_controlSource);
            m_controlSource = nullptr;
        }
        if (m_display != nullptr) {
            wl_display_destroy_clients(m_display);
            wl_display_destroy(m_display);
            m_display = nullptr;
        }
        for (int &fd : m_controlPipe) {
            if (fd >= 0) {
                ::close(fd);
                fd = -1;
            }
        }
    }

    wl_display *m_display = nullptr;
    wl_global *m_global = nullptr;
    wl_resource *m_managerResource = nullptr;
    wl_event_source *m_controlSource = nullptr;
    int m_controlPipe[2] = {-1, -1};
    std::thread m_thread;
};

} // namespace

class WindowManagerDispatchTest : public QObject
{
    Q_OBJECT

private slots:
    void receivesEventWhenQtDispatchSourcesAreDelayed()
    {
        EnvironmentGuard runtimeGuard("XDG_RUNTIME_DIR");
        EnvironmentGuard displayGuard("WAYLAND_DISPLAY");
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        QVERIFY(QFile::setPermissions(runtime.path(),
                                     QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                         | QFileDevice::ExeOwner));
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(runtime.path()));
        qputenv("WAYLAND_DISPLAY", QByteArrayLiteral("wayland-moko-dispatch-test"));

        FakeWaylandServer server;
        QVERIFY(server.start(qgetenv("WAYLAND_DISPLAY")));

        WindowManager manager;
        QVERIFY(manager.connected());
        QVERIFY(manager.m_notifier != nullptr);
        manager.m_notifier->setEnabled(false);
        manager.m_waylandDispatchTimer.stop();

        QSignalSpy powerMenuSpy(&manager, &WindowManager::powerMenuRequested);
        QVERIFY(powerMenuSpy.isValid());
        for (int index = 0; index < 20; ++index)
            QVERIFY(server.sendPowerMenu());
        QTRY_COMPARE_WITH_TIMEOUT(powerMenuSpy.count(), 20, 2000);
    }
};

QTEST_GUILESS_MAIN(WindowManagerDispatchTest)

#include "test_windowmanager_dispatch.moc"
