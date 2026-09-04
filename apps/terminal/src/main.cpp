#include "terminalview.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
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
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Terminal"));
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
        qCritical("MOKO Terminal refuses to run as root.");
        return 77;
    }
    if (validationMode)
        qputenv("MOKO_TERMINAL_SMOKE", QByteArrayLiteral("1"));

    qmlRegisterType<TerminalView>("Moko.Terminal", 1, 0, "TerminalView");
    bool qmlWarningsFound = false;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoTerminal"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (validationMode) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(screenshotPath.isEmpty() ? 900 : 1800, &app, [&, window]() {
            const bool saved = screenshotPath.isEmpty()
                || (window && QFileInfo(screenshotPath).dir().exists()
                    && window->grabWindow().save(screenshotPath));
            app.exit(qmlWarningsFound || !saved ? 2 : 0);
        });
    }
    return app.exec();
}
