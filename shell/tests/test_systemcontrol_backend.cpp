#include "systemcontrol.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

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
    void controlsFixtureBacklightBatteryAndAudio();
};

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
    writeFile(battery + QStringLiteral("/energy_full"), "8000\n");
    writeFile(battery + QStringLiteral("/energy_full_design"), "10000\n");
    writeFile(battery + QStringLiteral("/energy_now"), "6000\n");
    writeFile(battery + QStringLiteral("/power_now"), "2000\n");

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
