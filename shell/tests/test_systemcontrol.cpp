#include "systemcontrolutils.h"

#include <QTest>

class SystemControlTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesAudioEndpoints();
    void parsesAudioLevels();
    void decodesNetworkNames();
    void calculatesPercentages();
    void requiresCompleteNetworkConfiguration();
    void normalizesLargeWifiScans();
};

void SystemControlTest::parsesAudioEndpoints()
{
    const QString status = QStringLiteral(
        "Audio\n"
        " Sinks:\n"
        "  *   45. Built-in Audio Analog Stereo [vol: 0.72]\n"
        "      51. HDMI / DisplayPort 2 [vol: 1.00]\n"
        " Sources:\n"
        "  *   46. Internal Microphone [vol: 0.38]\n"
        " Filters:\n"
        "      77. Echo Canceller\n");

    const auto outputs = MokoSystemControl::parseWpctlEndpoints(
        status, QStringLiteral("Sinks"));
    QCOMPARE(outputs.size(), 2);
    QCOMPARE(outputs.at(0).id, 45);
    QCOMPARE(outputs.at(0).name, QStringLiteral("Built-in Audio Analog Stereo"));
    QVERIFY(outputs.at(0).defaultDevice);
    QCOMPARE(outputs.at(1).id, 51);
    QVERIFY(!outputs.at(1).defaultDevice);

    const auto inputs = MokoSystemControl::parseWpctlEndpoints(
        status, QStringLiteral("Sources"));
    QCOMPARE(inputs.size(), 1);
    QCOMPARE(inputs.constFirst().name, QStringLiteral("Internal Microphone"));
}

void SystemControlTest::parsesAudioLevels()
{
    const auto audible = MokoSystemControl::parseWpctlVolume(
        QStringLiteral("Volume: 0.73"));
    QVERIFY(audible.valid);
    QCOMPARE(audible.percent, 73);
    QVERIFY(!audible.muted);

    const auto muted = MokoSystemControl::parseWpctlVolume(
        QStringLiteral("Volume: 1.25 [MUTED]"));
    QVERIFY(muted.valid);
    QCOMPARE(muted.percent, 125);
    QVERIFY(muted.muted);
    QVERIFY(!MokoSystemControl::parseWpctlVolume(QStringLiteral("unavailable")).valid);
}

void SystemControlTest::decodesNetworkNames()
{
    QCOMPARE(MokoSystemControl::decodeSsid(QByteArray("MOKO Lab")),
             QStringLiteral("MOKO Lab"));
    QCOMPARE(MokoSystemControl::decodeSsid(QByteArray::fromHex("436166e9")),
             QString::fromLatin1("Caf\xe9"));
}

void SystemControlTest::calculatesPercentages()
{
    QCOMPARE(MokoSystemControl::percentage(50, 200), 25);
    QCOMPARE(MokoSystemControl::percentage(500, 200), 100);
    QCOMPARE(MokoSystemControl::percentage(-1, 200), -1);
    QCOMPARE(MokoSystemControl::percentage(10, 0), -1);
}

void SystemControlTest::requiresCompleteNetworkConfiguration()
{
    QVERIFY(MokoSystemControl::networkConnectionReady(100, true, true, true, true));
    QVERIFY(!MokoSystemControl::networkConnectionReady(70, true, true, true, true));
    QVERIFY(!MokoSystemControl::networkConnectionReady(100, false, true, true, true));
    QVERIFY(!MokoSystemControl::networkConnectionReady(100, true, false, true, true));
    QVERIFY(!MokoSystemControl::networkConnectionReady(100, true, true, false, true));
    QVERIFY(!MokoSystemControl::networkConnectionReady(100, true, true, true, false));
}

void SystemControlTest::normalizesLargeWifiScans()
{
    QVariantList networks;
    for (int index = 0; index < 200; ++index) {
        networks.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("/ap/%1").arg(index)},
                                    {QStringLiteral("ssid"), QStringLiteral("Network %1").arg(index)},
                                    {QStringLiteral("strength"), index % 100},
                                    {QStringLiteral("secure"), index % 2 == 0}});
    }
    networks.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("/ap/duplicate")},
                                {QStringLiteral("ssid"), QStringLiteral("Network 5")},
                                {QStringLiteral("strength"), 99},
                                {QStringLiteral("secure"), true}});

    const QVariantList normalized = MokoSystemControl::normalizeWifiNetworks(
        networks, QStringLiteral("/ap/5"), true);
    QCOMPARE(normalized.size(), 200);
    const QVariantMap active = normalized.constFirst().toMap();
    QCOMPARE(active.value(QStringLiteral("id")).toString(), QStringLiteral("/ap/5"));
    QVERIFY(active.value(QStringLiteral("active")).toBool());
    QVERIFY(active.value(QStringLiteral("connected")).toBool());
}

QTEST_GUILESS_MAIN(SystemControlTest)

#include "test_systemcontrol.moc"
