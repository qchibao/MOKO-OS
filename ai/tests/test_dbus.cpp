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

private:
    QProcess m_daemon;
};

void AiDbusTests::initTestCase()
{
    const QString daemonPath = qEnvironmentVariable("MOKO_AI_DAEMON_PATH");
    QVERIFY2(!daemonPath.isEmpty(), "MOKO_AI_DAEMON_PATH is required");
    m_daemon.setProgram(daemonPath);
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

void AiDbusTests::cleanupTestCase()
{
    if (m_daemon.state() != QProcess::NotRunning) {
        m_daemon.terminate();
        if (!m_daemon.waitForFinished(1000)) {
            m_daemon.kill();
            m_daemon.waitForFinished();
        }
    }
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

    const QDBusReply<QVariantMap> response =
        daemon.call(QStringLiteral("request"), QStringLiteral("system overview"));
    QVERIFY2(response.isValid(), qPrintable(response.error().message()));
    QVERIFY(response.value().value(QStringLiteral("ok")).toBool());

    const QDBusReply<QVariantMap> denied =
        daemon.call(QStringLiteral("request"), QStringLiteral("execute shell command"));
    QVERIFY2(denied.isValid(), qPrintable(denied.error().message()));
    QVERIFY(!denied.value().value(QStringLiteral("ok")).toBool());
}

QTEST_GUILESS_MAIN(AiDbusTests)
#include "test_dbus.moc"
