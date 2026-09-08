#include "applicationiconprovider.h"
#include "applicationregistry.h"
#include "applicationservice.h"
#include "aicontroller.h"
#include "notificationmodel.h"
#include "screenshotcontroller.h"
#include "sessionlifecycle.h"
#include "systemcontrol.h"
#include "windowmanager.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDBusConnection>
#include <QDBusError>
#include <QFile>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSet>
#include <QTimer>

namespace {

void writeShellSurfaceEvent(const QString &state, const QString &appId)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_SHELL_SURFACE state=%1 app_id=%2\n").arg(state, appId).toUtf8());
    file.flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("MOKO");
    QGuiApplication::setOrganizationDomain("moko.asia");
    QGuiApplication::setApplicationName("MOKO Shell");
    QGuiApplication::setDesktopFileName("org.moko.Shell");

    QQuickStyle::setStyle("Basic");

    QCommandLineParser parser;
    parser.setApplicationDescription("MOKO Shell Developer Preview");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"smoke-test", "Load the shell, fail on QML warnings, then exit."});
    parser.addOption({"windowed", "Run the developer preview in a window."});
    parser.addOption({"control-center", "Open Control Center for validation."});
    parser.addOption({"control-center-page", "Open a Control Center page (0-4).", "page", "0"});
    parser.addOption({"notification-center", "Open Notification Center for validation."});
    parser.addOption({"shutdown-preview", "Render the shutdown blackout for validation."});
    parser.addOption({"screenshot", "Save a preview screenshot and exit.", "path"});
    parser.addOption({"size", "Set the preview size, for example 1280x720.", "widthxheight"});
    parser.addOption({"application-dir", "Read applications from this directory (repeatable).", "path"});
    parser.process(app);

    const bool smokeTest = parser.isSet("smoke-test");
    const QString screenshotPath = parser.value("screenshot");
    bool qmlWarningsFound = false;

    ApplicationRegistry applicationRegistry(nullptr, parser.values("application-dir"));
    ApplicationFilterModel launcherApplications;
    launcherApplications.setSourceModel(&applicationRegistry);
    ApplicationFilterModel dockApplications;
    dockApplications.setPinnedOnly(true);
    dockApplications.setSourceModel(&applicationRegistry);
    ApplicationService applicationService(&applicationRegistry);
    AiController aiController;
    NotificationModel notificationModel;
    ScreenshotController screenshotController;
    SystemControl systemControl;
    WindowManager windowManager;
    SessionLifecycle::Options sessionOptions;
    // Preview mode drives the lifecycle locally; an unrelated system-bus
    // PrepareForShutdown(false) must not cancel the rendered blackout.
    sessionOptions.observeLogind = !parser.isSet("shutdown-preview");
    SessionLifecycle sessionLifecycle({
        .refreshSystem = [&systemControl] { systemControl.refresh(); },
        .refreshAi = [&aiController] { aiController.refreshConnection(); },
        .refreshWindowManager = [&windowManager] { return windowManager.refreshConnection(); },
        .readState = [&systemControl, &aiController, &windowManager] {
            return SessionLifecycle::State{
                .compositorConnected = windowManager.connected(),
                .desktopProtocolAvailable = windowManager.desktopProtocolAvailable(),
                .browserMapped = windowManager.isRunning(QStringLiteral("org.moko.Browser")),
                .aiConnected = aiController.connected(),
                .aiProviderAvailable = aiController.providerAvailable(),
                .networkManagerAvailable = systemControl.networkManagerAvailable(),
                .wifiAvailable = systemControl.wifiAvailable(),
                .wifiEnabled = systemControl.wifiEnabled(),
                .wifiConnected = !systemControl.activeSsid().isEmpty(),
                .bluezServiceAvailable = systemControl.bluezServiceAvailable(),
                .bluetoothAvailable = systemControl.bluetoothAvailable(),
                .bluetoothPowered = systemControl.bluetoothPowered(),
                .audioAvailable = systemControl.audioAvailable(),
                .inputProtocolAvailable = windowManager.inputProtocolAvailable(),
                .touchpadCount = windowManager.touchpadCount(),
                .batteryAvailable = systemControl.batteryAvailable(),
                .brightnessAvailable = systemControl.brightnessAvailable(),
                .powerModeAvailable = systemControl.powerModeAvailable(),
            };
        },
    }, sessionOptions);
    QObject::connect(&windowManager,
                     &WindowManager::brightnessStepRequested,
                     &systemControl,
                     &SystemControl::adjustBrightness);
    QObject::connect(&sessionLifecycle,
                     &SessionLifecycle::shutdownBlackoutPrepared,
                     &windowManager,
                     [&windowManager, &sessionLifecycle] {
                         const bool compositorRequestSent = windowManager.prepareShutdown();
                         // The protocol ACK is preferred, but a physical or
                         // emulated backend can drop the presentation event
                         // while logind is already waiting. The Shell has an
                         // opaque black overlay of its own, so use a bounded
                         // fallback to keep shutdown deterministic without
                         // releasing the inhibitor before the fade completes.
                         const int fallbackDelayMs = compositorRequestSent ? 2000 : 750;
                         QTimer::singleShot(fallbackDelayMs, &sessionLifecycle, [&sessionLifecycle] {
                             if (sessionLifecycle.shuttingDown())
                                 sessionLifecycle.notifyShutdownBlackoutPresented();
                         });
                     });
    QObject::connect(&sessionLifecycle,
                     &SessionLifecycle::shuttingDownChanged,
                     &app,
                     [&sessionLifecycle] {
                         if (sessionLifecycle.shuttingDown())
                             sessionLifecycle.notifyShutdownBlackoutPrepared();
                     });
    QObject::connect(&windowManager,
                     &WindowManager::shutdownBlackoutPresented,
                     &sessionLifecycle,
                     &SessionLifecycle::notifyShutdownBlackoutPresented);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        if (!sessionBus.registerService(QStringLiteral("org.moko.Applications1"))) {
            qWarning("Could not own org.moko.Applications1: %s",
                     qPrintable(sessionBus.lastError().message()));
        } else if (!sessionBus.registerObject(QStringLiteral("/org/moko/Applications1"),
                                              &applicationService,
                                              QDBusConnection::ExportAllSlots)) {
            qWarning("Could not export MOKO application service: %s",
                     qPrintable(sessionBus.lastError().message()));
        }
        if (!sessionBus.registerService(QStringLiteral("org.freedesktop.Notifications"))) {
            qWarning("Could not own org.freedesktop.Notifications: %s",
                     qPrintable(sessionBus.lastError().message()));
        } else if (!sessionBus.registerObject(QStringLiteral("/org/freedesktop/Notifications"),
                                              &notificationModel,
                                              QDBusConnection::ExportAllSlots
                                                  | QDBusConnection::ExportAllSignals)) {
            qWarning("Could not export MOKO notification service: %s",
                     qPrintable(sessionBus.lastError().message()));
        }
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("moko-app-icon"), new ApplicationIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoApplicationRegistry"),
                                             &applicationRegistry);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoLauncherApplications"),
                                             &launcherApplications);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoDockApplications"),
                                             &dockApplications);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoAiController"),
                                             &aiController);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoNotificationModel"),
                                             &notificationModel);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoScreenshotController"),
                                             &screenshotController);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoSystemControl"),
                                             &systemControl);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoWindowManager"),
                                             &windowManager);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoSessionLifecycle"),
                                             &sessionLifecycle);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        if (!warnings.isEmpty()) {
            qmlWarningsFound = true;
        }
    });
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MokoShell", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 1;

    if (parser.isSet("control-center")) {
        window->setProperty("activeSystemPanel", QStringLiteral("control-center"));
        window->setProperty("launcherVisible", false);
        window->setProperty("aiVisible", false);
        window->setProperty("controlCenterVisible", true);
        bool pageOk = false;
        const int page = parser.value("control-center-page").toInt(&pageOk);
        if (pageOk && page >= 0 && page <= 4)
            window->setProperty("controlCenterPage", page);
    }
    if (parser.isSet("notification-center")) {
        notificationModel.Notify(QStringLiteral("MOKO Browser"), 0, {},
                                 QStringLiteral("Download complete"),
                                 QStringLiteral("The file is available in Downloads."),
                                 {QStringLiteral("open"), QStringLiteral("Open")}, {}, 0);
        notificationModel.Notify(QStringLiteral("MOKO OS"), 0, {},
                                 QStringLiteral("Screenshot saved"),
                                 QStringLiteral("Your screenshot was saved in Pictures."),
                                 {}, {}, 0);
        window->setProperty("activeSystemPanel", QStringLiteral("notification-center"));
        window->setProperty("launcherVisible", false);
        window->setProperty("aiVisible", false);
        window->setProperty("controlCenterVisible", false);
        window->setProperty("notificationCenterVisible", true);
    }
    if (parser.isSet("shutdown-preview")) {
        QTimer::singleShot(100, &sessionLifecycle, [&sessionLifecycle] {
            sessionLifecycle.handlePrepareForShutdown(true);
        });
    }

    // BOOTSTRAP: Cage does not raise independent top-levels above the fullscreen shell.
    QSet<QString> runningApplications;
    if (!windowManager.connected()) {
        QObject::connect(&applicationRegistry,
                         &ApplicationRegistry::applicationRunning,
                         &app,
                         [window, &runningApplications](const QString &appId, const QString &) {
                             runningApplications.insert(appId);
                             QTimer::singleShot(250, window, [window, &runningApplications, appId]() {
                                 if (!runningApplications.contains(appId))
                                     return;
                                 window->hide();
                                 writeShellSurfaceEvent(QStringLiteral("hidden"), appId);
                             });
                         });
        QObject::connect(&applicationRegistry,
                         &ApplicationRegistry::applicationStopped,
                         &app,
                         [window, &runningApplications](const QString &appId, const QString &, int) {
                             runningApplications.remove(appId);
                             if (!runningApplications.isEmpty())
                                 return;
                             window->showFullScreen();
                             window->requestActivate();
                             writeShellSurfaceEvent(QStringLiteral("shown"), appId);
                         });
    }

    if (smokeTest || !screenshotPath.isEmpty()) {
        const QStringList sizeParts = parser.value("size").split('x');
        if (window && sizeParts.size() == 2) {
            bool widthOk = false;
            bool heightOk = false;
            const int width = sizeParts.at(0).toInt(&widthOk);
            const int height = sizeParts.at(1).toInt(&heightOk);
            if (widthOk && heightOk && width > 0 && height > 0) {
                window->resize(width, height);
            }
        }
        QTimer::singleShot(1200, &app, [&, window]() {
            bool screenshotSaved = true;
            if (!screenshotPath.isEmpty()) {
                const QFileInfo output(screenshotPath);
                screenshotSaved = output.dir().exists() && window && window->grabWindow().save(screenshotPath);
            }
            app.exit(qmlWarningsFound || !screenshotSaved ? 2 : 0);
        });
    }

    return app.exec();
}
