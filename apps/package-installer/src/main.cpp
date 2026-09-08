#include "packageinstaller.h"
#include "livemarker.h"

#include <QCommandLineParser>
#include <QDir>
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
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Package Installer"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.moko.PackageInstaller"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Load the installer UI and exit.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a validation screenshot."), QStringLiteral("path")});
    parser.addPositionalArgument(QStringLiteral("package"), QStringLiteral("A local package path."));
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    const QString screenshotPath = parser.value(QStringLiteral("screenshot"));
    const bool validationMode = smokeTest || !screenshotPath.isEmpty();
    if (geteuid() == 0 && !validationMode) {
        qCritical("MOKO Package Installer refuses to run as root.");
        return 77;
    }

    PackageInstaller installer;
    if (!parser.positionalArguments().isEmpty()) {
        installer.inspect(parser.positionalArguments().constFirst());
        writeMokoLiveEvent(
            QStringLiteral("MOKO_PACKAGE_INSPECT state=%1 type=%2 package=%3 version=%4 architecture=%5 uid=%6")
                .arg(installer.state(), installer.packageType(),
                     installer.packageName().left(80).replace(u' ', u'_'),
                     installer.version().left(80).replace(u' ', u'_'),
                     installer.architecture().left(32).replace(u' ', u'_'))
                .arg(geteuid()));
    }
    QObject::connect(&installer, &PackageInstaller::stateChanged, &app, [&installer] {
        writeMokoLiveEvent(QStringLiteral("MOKO_PACKAGE state=%1 detail=%2 uid=%3")
                               .arg(installer.state(),
                                    installer.statusMessage().left(180).replace(u' ', u'_'))
                               .arg(geteuid()));
    });
    writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.PackageInstaller state=ready"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoPackageInstaller"), &installer);
    bool qmlWarningsFound = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoPackageInstaller"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (validationMode) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(900, &app, [&, window]() {
            const bool saved = screenshotPath.isEmpty()
                || (window && QFileInfo(screenshotPath).dir().exists()
                    && window->grabWindow().save(screenshotPath));
            app.exit(qmlWarningsFound || !saved ? 2 : 0);
        });
    }
    return app.exec();
}
