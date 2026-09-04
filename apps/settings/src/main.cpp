#include "systemsettings.h"
#include "livemarker.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <unistd.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("MOKO"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("moko.asia"));
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Settings"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Load the QML UI and exit.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a validation screenshot and exit."),
                      QStringLiteral("path")});
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    const QString screenshotPath = parser.value(QStringLiteral("screenshot"));
    const bool validationMode = smokeTest || !screenshotPath.isEmpty();
    if (geteuid() == 0 && !validationMode) {
        qCritical("MOKO Settings refuses to run as root.");
        return 77;
    }

    SystemSettings settings;
    bool qmlWarningsFound = false;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoSettings"), &settings);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoSettings"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;
    writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.Settings state=ready detail=system-data-loaded"));
    if (validationMode) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(1000, &app, [&, window]() {
            const bool saved = screenshotPath.isEmpty()
                || (window && QFileInfo(screenshotPath).dir().exists()
                    && window->grabWindow().save(screenshotPath));
            app.exit(qmlWarningsFound || !saved ? 2 : 0);
        });
    }
    return app.exec();
}
