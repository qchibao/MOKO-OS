#include "aicontroller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QtTest>

class AiDbusTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void callsRoundTripOverDbus();
    void controllerStateFlow();
    void controllerTracksDaemonRestart();

private:
    void startDaemon();
    void stopDaemon();

    QProcess m_daemon;
    QString m_daemonPath;
};

void AiDbusTests::startDaemon()
{
    m_daemon.setProgram(m_daemonPath);
    m_daemon.setArguments({QStringLiteral("--smoke-test")});
    m_daemon.start();
    QVERIFY(m_daemon.waitForStarted(2000));

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2500) {
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(
                QStringLiteral("org.moko.AI1"))) {
            return;
        }
        QThread::msleep(25);
    }
    QFAIL("moko-ai-daemon did not register org.moko.AI1");
}

void AiDbusTests::stopDaemon()
{
    if (m_daemon.state() != QProcess::NotRunning) {
        m_daemon.terminate();
        if (!m_daemon.waitForFinished(1000)) {
            m_daemon.kill();
            m_daemon.waitForFinished();
        }
    }
}

void AiDbusTests::initTestCase()
{
    m_daemonPath = qEnvironmentVariable("MOKO_AI_DAEMON_PATH");
    QVERIFY2(!m_daemonPath.isEmpty(), "MOKO_AI_DAEMON_PATH is required");
    startDaemon();
}

void AiDbusTests::cleanupTestCase()
{
    stopDaemon();
}

void AiDbusTests::callsRoundTripOverDbus()
{
    QDBusInterface daemon(QStringLiteral("org.moko.AI1"),
                          QStringLiteral("/org/moko/AI1"),
                          QStringLiteral("org.moko.AI1"),
                          QDBusConnection::sessionBus());
    QVERIFY2(daemon.isValid(), qPrintable(daemon.lastError().message()));

    const QDBusReply<QString> ping = daemon.call(QStringLiteral("ping"));
    QVERIFY2(ping.isValid(), qPrintable(ping.error().message()));
    QCOMPARE(ping.value(), QStringLiteral("pong"));

    const QDBusReply<QVariantMap> summary = daemon.call(QStringLiteral("getSystemSummary"));
    QVERIFY2(summary.isValid(), qPrintable(summary.error().message()));
    QVERIFY(!summary.value().value(QStringLiteral("kernel")).toString().isEmpty());

    const QDBusReply<QVariantMap> provider = daemon.call(QStringLiteral("providerStatus"));
    QVERIFY2(provider.isValid(), qPrintable(provider.error().message()));
    QVERIFY(provider.value().value(QStringLiteral("available")).toBool());
    QCOMPARE(provider.value().value(QStringLiteral("name")).toString(),
             QStringLiteral("local-stub"));

    const QDBusReply<QVariantMap> response =
        daemon.call(QStringLiteral("request"), QStringLiteral("system overview"));
    QVERIFY2(response.isValid(), qPrintable(response.error().message()));
    QVERIFY(response.value().value(QStringLiteral("ok")).toBool());

    const QDBusReply<QVariantMap> denied =
        daemon.call(QStringLiteral("request"), QStringLiteral("execute shell command"));
    QVERIFY2(denied.isValid(), qPrintable(denied.error().message()));
    QVERIFY(!denied.value().value(QStringLiteral("ok")).toBool());
}

void AiDbusTests::controllerStateFlow()
{
    AiController controller;
    QTRY_VERIFY_WITH_TIMEOUT(controller.connected(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.providerAvailable(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Ready"), 2000);

    controller.submit(QStringLiteral("system information"));
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Response"), 2000);
    QVERIFY(!controller.failed());
    QVERIFY(!controller.response().isEmpty());

    controller.submit(QStringLiteral("compose a poem"));
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Failed"), 2000);
    QVERIFY(controller.failed());
}

void AiDbusTests::controllerTracksDaemonRestart()
{
    AiController controller;
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Ready"), 2000);

    stopDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.connected(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.providerAvailable(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Provider unavailable"), 2000);

    startDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(controller.connected(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.providerAvailable(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("Ready"), 2000);
}

QTEST_GUILESS_MAIN(AiDbusTests)
#include "test_dbus.moc"
