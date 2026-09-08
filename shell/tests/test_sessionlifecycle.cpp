#include "sessionlifecycle.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class SessionLifecycleTest final : public QObject
{
    Q_OBJECT

private slots:
    void restoresCapabilitiesAndBrowserMapping();
    void reportsMissingRequiredState();
    void acceptsHardwareThatWasAlreadyAbsent();
    void fadesBeforeReleasingShutdown();
};

void SessionLifecycleTest::restoresCapabilitiesAndBrowserMapping()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    qputenv("MOKO_LIVE_LAUNCH_EVENTS", events.toUtf8());

    SessionLifecycle::State state{
        .compositorConnected = true,
        .desktopProtocolAvailable = true,
        .browserMapped = true,
        .aiConnected = true,
        .aiProviderAvailable = true,
        .networkManagerAvailable = true,
        .wifiAvailable = true,
        .wifiEnabled = true,
        .wifiConnected = true,
        .bluezServiceAvailable = true,
        .bluetoothAvailable = true,
        .bluetoothPowered = true,
        .audioAvailable = true,
        .inputProtocolAvailable = true,
        .touchpadCount = 1,
        .batteryAvailable = true,
        .brightnessAvailable = true,
        .powerModeAvailable = true,
    };
    int systemRefreshes = 0;
    int aiRefreshes = 0;
    int windowRefreshes = 0;
    SessionLifecycle lifecycle({
        .refreshSystem = [&] {
            ++systemRefreshes;
            state.networkManagerAvailable = true;
            state.wifiAvailable = true;
            state.wifiEnabled = true;
            state.wifiConnected = true;
            state.bluezServiceAvailable = true;
            state.bluetoothAvailable = true;
            state.bluetoothPowered = true;
            state.audioAvailable = true;
            state.batteryAvailable = true;
            state.brightnessAvailable = true;
            state.powerModeAvailable = true;
        },
        .refreshAi = [&] {
            ++aiRefreshes;
            state.aiConnected = true;
            state.aiProviderAvailable = true;
        },
        .refreshWindowManager = [&] {
            ++windowRefreshes;
            state.compositorConnected = true;
            state.desktopProtocolAvailable = true;
            state.browserMapped = true;
            state.inputProtocolAvailable = true;
            state.touchpadCount = 1;
            return true;
        },
        .readState = [&] { return state; },
    }, SessionLifecycle::Options{.observeLogind = false,
                                 .healthTimeoutMs = 100,
                                 .healthCheckIntervalMs = 1});
    QSignalSpy health(&lifecycle, &SessionLifecycle::resumeHealthReported);

    lifecycle.handlePrepareForSleep(true);
    QVERIFY(lifecycle.preparingForSleep());
    state = {};
    lifecycle.handlePrepareForSleep(false);

    QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 500);
    QVERIFY(health.constFirst().constFirst().toBool());
    QVERIFY(!lifecycle.preparingForSleep());
    QCOMPARE(systemRefreshes, 1);
    QCOMPARE(aiRefreshes, 1);
    QCOMPARE(windowRefreshes, 1);

    QFile file(events);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.contains("MOKO_SLEEP state=preparing compositor=1 browser_running=1"));
    QVERIFY(contents.contains("MOKO_SLEEP state=resumed"));
    QVERIFY(contents.contains(
        "MOKO_RESUME_HEALTH result=pass desktop_protocol=1 compositor=1 "));
    QVERIFY(contents.contains("browser_expected=1 browser_mapped=1"));
    qunsetenv("MOKO_LIVE_LAUNCH_EVENTS");
}

void SessionLifecycleTest::reportsMissingRequiredState()
{
    SessionLifecycle::State state{.compositorConnected = true,
                                  .desktopProtocolAvailable = true,
                                  .browserMapped = true};
    SessionLifecycle lifecycle({
        .refreshWindowManager = [&] { return true; },
        .readState = [&] { return state; },
    }, SessionLifecycle::Options{.observeLogind = false,
                                 .healthTimeoutMs = 15,
                                 .healthCheckIntervalMs = 2});
    QSignalSpy health(&lifecycle, &SessionLifecycle::resumeHealthReported);

    lifecycle.handlePrepareForSleep(true);
    state.browserMapped = false;
    lifecycle.handlePrepareForSleep(false);

    QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 500);
    QVERIFY(!health.constFirst().constFirst().toBool());
}

void SessionLifecycleTest::acceptsHardwareThatWasAlreadyAbsent()
{
    SessionLifecycle::State state;
    SessionLifecycle lifecycle({
        .refreshSystem = [] {},
        .refreshAi = [] {},
        .refreshWindowManager = [] { return false; },
        .readState = [&] { return state; },
    }, SessionLifecycle::Options{.observeLogind = false,
                                 .healthTimeoutMs = 20,
                                 .healthCheckIntervalMs = 1});
    QSignalSpy health(&lifecycle, &SessionLifecycle::resumeHealthReported);

    lifecycle.handlePrepareForSleep(true);
    lifecycle.handlePrepareForSleep(false);

    QTRY_COMPARE_WITH_TIMEOUT(health.size(), 1, 500);
    QVERIFY(health.constFirst().constFirst().toBool());
}

void SessionLifecycleTest::fadesBeforeReleasingShutdown()
{
    QCOMPARE(SessionLifecycle::Options{}.shutdownFadeDelayMs, 0);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString events = directory.filePath(QStringLiteral("events"));
    qputenv("MOKO_LIVE_LAUNCH_EVENTS", events.toUtf8());

    SessionLifecycle lifecycle({}, SessionLifecycle::Options{
                                       .observeLogind = false,
                                       .healthTimeoutMs = 20,
                                       .healthCheckIntervalMs = 1,
                                       .shutdownFadeDelayMs = 5,
                                       .shutdownReleaseGraceMs = 1,
                                   });
    QSignalSpy changed(&lifecycle, &SessionLifecycle::shuttingDownChanged);
    QSignalSpy prepared(&lifecycle, &SessionLifecycle::shutdownBlackoutPrepared);
    QSignalSpy complete(&lifecycle, &SessionLifecycle::shutdownFadeCompleted);

    lifecycle.handlePrepareForShutdown(true);
    QVERIFY(lifecycle.shuttingDown());
    QCOMPARE(changed.size(), 1);
    QTest::qWait(10);
    QCOMPARE(complete.size(), 0);

    lifecycle.notifyShutdownBlackoutPrepared();
    QCOMPARE(prepared.size(), 1);
    lifecycle.notifyShutdownBlackoutPresented();
    QTRY_COMPARE_WITH_TIMEOUT(complete.size(), 1, 200);

    QFile file(events);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.contains("MOKO_SHUTDOWN_VISUAL state=fading"));
    QVERIFY(contents.contains("MOKO_SHUTDOWN_VISUAL state=blackout"));
    QVERIFY(contents.contains("MOKO_SHUTDOWN_VISUAL state=ready"));

    lifecycle.handlePrepareForShutdown(false);
    QVERIFY(!lifecycle.shuttingDown());
    QCOMPARE(changed.size(), 2);
    qunsetenv("MOKO_LIVE_LAUNCH_EVENTS");
}

QTEST_GUILESS_MAIN(SessionLifecycleTest)

#include "test_sessionlifecycle.moc"
