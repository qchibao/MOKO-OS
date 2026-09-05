#include "applicationiconprovider.h"
#include "applicationregistry.h"
#include "applicationservice.h"
#include "aicontroller.h"
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
    WindowManager windowManager;

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
    engine.rootContext()->setContextProperty(QStringLiteral("mokoWindowManager"),
                                             &windowManager);
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
