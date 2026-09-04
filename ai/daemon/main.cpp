#include "aidaemon.h"

#include "aiactions.h"
#include "localstubprovider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <memory>
#include <unistd.h>

namespace {

void writeReadyEvent(const QString &provider)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_AI_DAEMON state=ready uid=%1 provider=%2\n")
                   .arg(geteuid())
                   .arg(provider)
                   .toUtf8());
    file.flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MOKO"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("moko.asia"));
    QCoreApplication::setApplicationName(QStringLiteral("MOKO AI Daemon"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"),
                      QStringLiteral("Register the D-Bus service briefly, then exit.")});
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));

    if (geteuid() == 0 && !smokeTest) {
        qCritical("moko-ai-daemon refuses to run as root.");
        return 77;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("moko-ai-daemon requires a user session D-Bus.");
        return 2;
    }

    AiActions actions;
    auto provider = std::make_unique<LocalStubProvider>();
    const QString providerName = provider->name();
    AiDaemon daemon(&actions, std::move(provider));

    if (!bus.registerService(QStringLiteral("org.moko.AI1"))) {
        qCritical("Could not own org.moko.AI1: %s",
                  qPrintable(bus.lastError().message()));
        return 3;
    }
    if (!bus.registerObject(QStringLiteral("/org/moko/AI1"), &daemon,
                            QDBusConnection::ExportAllSlots)) {
        qCritical("Could not export /org/moko/AI1: %s",
                  qPrintable(bus.lastError().message()));
        return 4;
    }

    writeReadyEvent(providerName);
    if (smokeTest)
        QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    return app.exec();
}
