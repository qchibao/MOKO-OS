#include "filemodel.h"
#include "livemarker.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <unistd.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("MOKO"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("moko.asia"));
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Files"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.moko.Files"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Load the QML UI and exit.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a validation screenshot and exit."),
                      QStringLiteral("path")});
    parser.addPositionalArgument(QStringLiteral("location"),
                                 QStringLiteral("Directory path or file URI to open."),
                                 QStringLiteral("[location]"));
    parser.process(app);

    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    const QString screenshotPath = parser.value(QStringLiteral("screenshot"));
    const bool validationMode = smokeTest || !screenshotPath.isEmpty();
    if (geteuid() == 0 && !validationMode) {
        qCritical("MOKO Files refuses to run as root.");
        return 77;
    }

    QString initialPath;
    if (!parser.positionalArguments().isEmpty()) {
        const QString location = parser.positionalArguments().constFirst();
        const QUrl url(location);
        initialPath = url.isLocalFile() ? url.toLocalFile() : location;
    }
    FileModel files(nullptr, initialPath);
    bool qmlWarningsFound = false;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoFiles"), &files);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoFiles"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.Files state=ready detail=location-loaded"));
    QString locationMarker = files.displayPath();
    locationMarker.replace(u' ', u'_');
    writeMokoLiveEvent(QStringLiteral("MOKO_FILES_LOCATION location=%1 count=%2 uid=%3")
                           .arg(locationMarker.left(160))
                           .arg(files.rowCount())
                           .arg(geteuid()));

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
