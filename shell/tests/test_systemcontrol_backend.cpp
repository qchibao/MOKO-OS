#include "systemcontrol.h"

#include <QDir>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

void writeFile(const QString &path, const QByteArray &contents, QFile::Permissions permissions = {})
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
    file.close();
    if (permissions != QFile::Permissions{})
        QVERIFY(file.setPermissions(permissions));
}

} // namespace

class SystemControlBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void defersInitialBackendProbe();
    void networkRefreshDoesNotBlockTheEventLoop();
    void controlsFixtureBacklightBatteryAndAudio();
};

void SystemControlBackendTest::defersInitialBackendProbe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString commandLog = directory.filePath(QStringLiteral("wpctl.log"));
    const QString fakeWpctl = directory.filePath(QStringLiteral("wpctl"));
    writeFile(fakeWpctl,
              QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$*\" >> '%1'\nexit 0\n")
                  .arg(commandLog)
                  .toUtf8(),
              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    qputenv("MOKO_SYSFS_ROOT", directory.path().toUtf8());
    qputenv("MOKO_WPCTL", fakeWpctl.toUtf8());
    SystemControl control(nullptr, true);

    QVERIFY(!QFile::exists(commandLog));
    control.refresh();
    QVERIFY(QFile::exists(commandLog));

    qunsetenv("MOKO_SYSFS_ROOT");
    qunsetenv("MOKO_WPCTL");
}

void SystemControlBackendTest::networkRefreshDoesNotBlockTheEventLoop()
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.NetworkManager"),
                           QStringLiteral("/org/freedesktop/NetworkManager"),
                           QStringLiteral("org.freedesktop.NetworkManager"),
                           QDBusConnection::systemBus());
    QVERIFY2(manager.isValid(), qPrintable(manager.lastError().message()));
    manager.setTimeout(1000);
    QVERIFY(QDBusReply<void>(manager.call(QStringLiteral("SetTestDelay"), 300)).isValid());

    SystemControl control(nullptr, true);
    QSignalSpy changed(&control, &SystemControl::networkChanged);
    QElapsedTimer elapsed;
    elapsed.start();
    control.preloadNetwork();
    QVERIFY2(elapsed.elapsed() < 50, "Network preload blocked the GUI thread");

    bool timerFired = false;
    QTimer::singleShot(25, [&timerFired] { timerFired = true; });
    QTRY_VERIFY_WITH_TIMEOUT(timerFired, 150);
    QTRY_VERIFY_WITH_TIMEOUT(control.networkManagerAvailable(), 3000);
    QVERIFY(changed.count() > 0);
    QVERIFY(control.wifiAvailable());
    QVERIFY(control.wifiEnabled());
    QCOMPARE(control.wifiState(), QStringLiteral("Connected"));
    QCOMPARE(control.activeSsid(), QStringLiteral("MOKO Lab"));
    QCOMPARE(control.wifiNetworks().size(), 180);
    const QVariantMap activeNetwork = control.wifiNetworks().constFirst().toMap();
    QCOMPARE(activeNetwork.value(QStringLiteral("ssid")).toString(), QStringLiteral("MOKO Lab"));
    QVERIFY(activeNetwork.value(QStringLiteral("active")).toBool());
    QVERIFY(activeNetwork.value(QStringLiteral("connected")).toBool());

    QVERIFY(QDBusReply<void>(manager.call(QStringLiteral("SetConnectionReady"), false)).isValid());
    QVERIFY(QDBusReply<void>(manager.call(QStringLiteral("SetTestDelay"), 0)).isValid());
    control.preloadNetwork();
    QTRY_COMPARE_WITH_TIMEOUT(control.wifiState(), QStringLiteral("Connected locally"), 3000);
    QCOMPARE(control.activeSsid(), QStringLiteral("MOKO Lab"));
    const QVariantMap localNetwork = control.wifiNetworks().constFirst().toMap();
    QVERIFY(localNetwork.value(QStringLiteral("active")).toBool());
    QVERIFY(!localNetwork.value(QStringLiteral("connected")).toBool());

    QVERIFY(control.connectWifi(QStringLiteral("Test Network 001"), QStringLiteral("test-password")));
    QVERIFY(control.networkBusy());
    QTRY_VERIFY_WITH_TIMEOUT(
        control.operationMessage().contains(QStringLiteral("Finishing connection")), 1000);
    QVERIFY(QDBusReply<void>(manager.call(QStringLiteral("SetConnectionReady"), true)).isValid());
    control.preloadNetwork();
    QTRY_COMPARE_WITH_TIMEOUT(control.wifiState(), QStringLiteral("Connected"), 3000);
    QCOMPARE(control.activeSsid(), QStringLiteral("Test Network 001"));
    QTRY_VERIFY_WITH_TIMEOUT(!control.networkBusy(), 1000);
    QVERIFY(control.disconnectWifi());
    QTRY_COMPARE_WITH_TIMEOUT(control.wifiState(), QStringLiteral("Disconnected"), 1000);
    QVERIFY(!control.networkBusy());

    QVERIFY(QDBusReply<void>(manager.call(QStringLiteral("SetTestDelay"), 2000)).isValid());
    elapsed.restart();
    control.preloadNetwork();
    QCoreApplication::processEvents();
    control.preloadNetwork();
    QVERIFY2(elapsed.elapsed() < 100, "Overlapping network refreshes blocked the GUI thread");

    timerFired = false;
    QTimer::singleShot(25, [&timerFired] { timerFired = true; });
    QTRY_VERIFY_WITH_TIMEOUT(timerFired, 150);
    QTRY_VERIFY_WITH_TIMEOUT(
        control.operationMessage().contains(QStringLiteral("temporarily unavailable")), 2500);
    QVERIFY(control.networkManagerAvailable());
}

void SystemControlBackendTest::controlsFixtureBacklightBatteryAndAudio()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString backlight = directory.filePath(QStringLiteral("class/backlight/apple_backlight"));
    const QString battery = directory.filePath(QStringLiteral("class/power_supply/BAT0"));
    QVERIFY(QDir().mkpath(backlight));
    QVERIFY(QDir().mkpath(battery));
    writeFile(backlight + QStringLiteral("/max_brightness"), "1000\n");
    writeFile(backlight + QStringLiteral("/actual_brightness"), "400\n");
    writeFile(backlight + QStringLiteral("/brightness"), "400\n");
    writeFile(battery + QStringLiteral("/type"), "Battery\n");
    writeFile(battery + QStringLiteral("/capacity"), "72\n");
    writeFile(battery + QStringLiteral("/status"), "Discharging\n");
    writeFile(battery + QStringLiteral("/energy_full"), "8000000\n");
    writeFile(battery + QStringLiteral("/energy_full_design"), "10000000\n");
    writeFile(battery + QStringLiteral("/energy_now"), "6000000\n");
    writeFile(battery + QStringLiteral("/power_now"), "2000000\n");
    writeFile(battery + QStringLiteral("/technology"), "Li-ion\n");
    writeFile(battery + QStringLiteral("/cycle_count"), "321\n");
    const QString mains = directory.filePath(QStringLiteral("class/power_supply/AC"));
    QVERIFY(QDir().mkpath(mains));
    writeFile(mains + QStringLiteral("/type"), "Mains\n");
    writeFile(mains + QStringLiteral("/online"), "1\n");

    const QString commandLog = directory.filePath(QStringLiteral("wpctl.log"));
    const QString fakeWpctl = directory.filePath(QStringLiteral("wpctl"));
    writeFile(fakeWpctl,
              QStringLiteral(
                  "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >> '%1'\n"
                  "case \"$1\" in\n"
                  "  status) printf '%s\\n' 'Audio' ' Sinks:' '  * 45. Built-in Output [vol: 0.64]' ' Sources:' '  * 46. Built-in Microphone [vol: 0.35]' ;;\n"
                  "  get-volume) case \"$2\" in *SOURCE*) echo 'Volume: 0.35' ;; *) echo 'Volume: 0.64' ;; esac ;;\n"
                  "esac\n"
                  "exit 0\n")
                  .arg(commandLog)
                  .toUtf8(),
              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    qputenv("MOKO_SYSFS_ROOT", directory.path().toUtf8());
    qputenv("MOKO_WPCTL", fakeWpctl.toUtf8());
    qputenv("MOKO_BRIGHTNESSCTL", directory.filePath(QStringLiteral("missing-brightnessctl")).toUtf8());
    SystemControl control;

    QVERIFY(control.brightnessAvailable());
    QCOMPARE(control.brightness(), 40);
    QVERIFY(control.setBrightness(65));
    QFile brightnessFile(backlight + QStringLiteral("/brightness"));
    QVERIFY(brightnessFile.open(QIODevice::ReadOnly));
    QCOMPARE(brightnessFile.readAll(), QByteArray("650"));

    QVERIFY(control.batteryAvailable());
    QCOMPARE(control.batteryPercent(), 72);
    QCOMPARE(control.batteryHealth(), 80);
    QCOMPARE(control.batteryState(), QStringLiteral("Discharging"));
    QCOMPARE(control.batteryTime(), QStringLiteral("3 h 0 min"));
    QCOMPARE(control.batteryTechnology(), QStringLiteral("Li-ion"));
    QCOMPARE(control.batteryCycleCount(), QStringLiteral("321"));
    QCOMPARE(control.batteryEnergy(), QStringLiteral("6.0 / 8.0 Wh"));
    QVERIFY(control.externalPowerConnected());
    QVERIFY(!control.batteryCharging());

    writeFile(battery + QStringLiteral("/status"), "Charging\n");
    control.refresh();
    QVERIFY(control.externalPowerConnected());
    QVERIFY(control.batteryCharging());
    QCOMPARE(control.batteryState(), QStringLiteral("Charging"));

    writeFile(mains + QStringLiteral("/online"), "0\n");
    writeFile(battery + QStringLiteral("/status"), "Discharging\n");
    control.refresh();
    QVERIFY(!control.externalPowerConnected());
    QVERIFY(!control.batteryCharging());

    QVERIFY(control.audioAvailable());
    QCOMPARE(control.outputVolume(), 64);
    QCOMPARE(control.inputVolume(), 35);
    QCOMPARE(control.outputDevices().size(), 1);
    QCOMPARE(control.inputDevices().size(), 1);
    QVERIFY(control.setOutputVolume(73));
    QVERIFY(control.setOutputMuted(true));
    QVERIFY(control.setOutputDevice(45));
    QVERIFY(!control.setOutputDevice(999));
    QVERIFY(control.setInputVolume(42));
    QVERIFY(control.setInputMuted(true));
    QVERIFY(control.setInputDevice(46));

    QFile log(commandLog);
    QVERIFY(log.open(QIODevice::ReadOnly));
    const QByteArray invocations = log.readAll();
    QVERIFY(invocations.contains("set-volume @DEFAULT_AUDIO_SINK@ 0.73"));
    QVERIFY(invocations.contains("set-mute @DEFAULT_AUDIO_SINK@ 1"));
    QVERIFY(invocations.contains("set-default 45"));
    QVERIFY(invocations.contains("set-volume @DEFAULT_AUDIO_SOURCE@ 0.42"));
    QVERIFY(invocations.contains("set-default 46"));

    qunsetenv("MOKO_SYSFS_ROOT");
    qunsetenv("MOKO_WPCTL");
    qunsetenv("MOKO_BRIGHTNESSCTL");
}

QTEST_GUILESS_MAIN(SystemControlBackendTest)

#include "test_systemcontrol_backend.moc"
