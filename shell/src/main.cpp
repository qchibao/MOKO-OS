#include "applicationiconprovider.h"
#include "applicationregistry.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("MOKO");
    QGuiApplication::setOrganizationDomain("moko.asia");
    QGuiApplication::setApplicationName("MOKO Shell");

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

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("moko-app-icon"), new ApplicationIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoApplicationRegistry"),
                                             &applicationRegistry);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoLauncherApplications"),
                                             &launcherApplications);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoDockApplications"),
                                             &dockApplications);
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

    if (smokeTest || !screenshotPath.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
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
